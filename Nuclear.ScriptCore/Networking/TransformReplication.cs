using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using Nuclear.ECS;
using Nuclear.Math;

namespace Nuclear.Networking
{
    /// <summary>One configured UDP peer carrying full position snapshots for small games. Network IDs are assigned by the game.</summary>
    public sealed class TransformReplication : IDisposable
    {
        public const int MaximumEntities = 72;
        private const int Header = 12, Record = 16;
        private readonly Socket socket;
        private readonly Dictionary<uint, Entity> tracked = new();
        private readonly byte[] packet = new byte[Header + MaximumEntities * Record + 1];
        private uint sent, received;
        private bool hasReceived;

        public IPEndPoint LocalEndpoint => (IPEndPoint)socket.LocalEndPoint;

        public TransformReplication(IPEndPoint local)
        {
            socket = new Socket(local.AddressFamily, SocketType.Dgram, ProtocolType.Udp);
            try { socket.Bind(local); socket.Blocking = false; }
            catch { socket.Dispose(); throw; }
        }

        /// <summary>Sets the sole remote endpoint and starts a fresh snapshot sequence.</summary>
        public void Connect(IPEndPoint remote)
        {
            socket.Connect(remote);
            received = sent = 0;
            hasReceived = false;
        }

        public void Track(uint networkId, Entity entity) => tracked[networkId] = entity ?? throw new ArgumentNullException(nameof(entity));
        public void Untrack(uint networkId) => tracked.Remove(networkId);

        public void Publish()
        {
            var state = new Dictionary<uint, Vector3>();
            foreach (var (id, entity) in tracked) state.Add(id, entity.Position);
            Send(state);
        }

        /// <summary>Runs on the game thread. Only registered game entities are updated.</summary>
        public void Apply()
        {
            while (TryReceive(out var snapshot))
                foreach (var (id, position) in snapshot)
                    if (tracked.TryGetValue(id, out var entity)) entity.Position = position;
        }

        public void Send(IReadOnlyDictionary<uint, Vector3> state)
        {
            if (state.Count > MaximumEntities) throw new ArgumentOutOfRangeException(nameof(state));
            var data = packet.AsSpan();
            "NPS1"u8.CopyTo(data);
            BinaryPrimitives.WriteUInt32LittleEndian(data[4..], ++sent);
            BinaryPrimitives.WriteInt32LittleEndian(data[8..], state.Count);
            int offset = Header;
            foreach (var (id, p) in state)
            {
                if (!float.IsFinite(p.x) || !float.IsFinite(p.y) || !float.IsFinite(p.z)) throw new ArgumentException("Invalid position");
                BinaryPrimitives.WriteUInt32LittleEndian(data[offset..], id);
                BinaryPrimitives.WriteSingleLittleEndian(data[(offset + 4)..], p.x);
                BinaryPrimitives.WriteSingleLittleEndian(data[(offset + 8)..], p.y);
                BinaryPrimitives.WriteSingleLittleEndian(data[(offset + 12)..], p.z);
                offset += Record;
            }
            try { socket.Send(data[..offset]); }
            catch (SocketException e) when (e.SocketErrorCode == SocketError.WouldBlock) { }
        }

        /// <summary>Returns the next complete newer snapshot. Packet loss is repaired by the next full update.</summary>
        public bool TryReceive(out Dictionary<uint, Vector3> state)
        {
            state = null;
            if (!socket.Poll(0, SelectMode.SelectRead)) return false;
            int size;
            try { size = socket.Receive(packet); }
            catch (SocketException e) when (e.SocketErrorCode is SocketError.WouldBlock or SocketError.MessageSize) { return false; }
            var data = packet.AsSpan(0, size);
            if (size < Header || !data[..4].SequenceEqual("NPS1"u8)) return false;
            int count = BinaryPrimitives.ReadInt32LittleEndian(data[8..]);
            uint sequence = BinaryPrimitives.ReadUInt32LittleEndian(data[4..]);
            if ((uint)count > MaximumEntities || size != Header + count * Record || (hasReceived && unchecked((int)(sequence - received)) <= 0))
                return false;
            var values = new Dictionary<uint, Vector3>(count);
            for (int offset = Header; offset < size; offset += Record)
            {
                uint id = BinaryPrimitives.ReadUInt32LittleEndian(data[offset..]);
                var point = new Vector3(BinaryPrimitives.ReadSingleLittleEndian(data[(offset + 4)..]),
                    BinaryPrimitives.ReadSingleLittleEndian(data[(offset + 8)..]), BinaryPrimitives.ReadSingleLittleEndian(data[(offset + 12)..]));
                if (!float.IsFinite(point.x) || !float.IsFinite(point.y) || !float.IsFinite(point.z) || !values.TryAdd(id, point)) return false;
            }
            received = sequence;
            hasReceived = true;
            state = values;
            return true;
        }

        public void Dispose() => socket.Dispose();
    }
}

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Net;
using System.Threading;
using Nuclear.Math;
using Nuclear.Navigation;
using Nuclear.Networking;

static class GameplayTests
{
    static void Main()
    {
        var grid = new NavigationGrid(4, 3, 1, new Vector3(0, 0, 0));
        grid.SetBlocked(1, 0, true);
        var path = grid.FindPath(new Vector3(.5f, 0, .5f), new Vector3(3.5f, 0, .5f));
        if (path.Count != 6) throw new Exception("Unexpected detour");
        grid.SetBlocked(1, 1, true);
        grid.SetBlocked(1, 2, true);
        if (grid.FindPath(path[0], path[^1]).Count != 0) throw new Exception("Walked through barrier");

        using var authority = new TransformReplication(new IPEndPoint(IPAddress.Loopback, 0));
        using var peer = new TransformReplication(new IPEndPoint(IPAddress.Loopback, 0));
        authority.Connect(peer.LocalEndpoint);
        peer.Connect(authority.LocalEndpoint);
        var input = new Dictionary<uint, Vector3> { [7] = new Vector3(-2, 4, 6) };
        authority.Send(input);
        var timer = Stopwatch.StartNew();
        while (timer.ElapsedMilliseconds < 3000)
        {
            if (peer.TryReceive(out var state))
            {
                if (state.Count != 1 || state[7].x != -2 || state[7].z != 6) throw new Exception("Incorrect replica");
                Console.WriteLine("Navigation detour, unreachable goal and UDP transform replication passed.");
                return;
            }
            Thread.Sleep(1);
        }
        throw new TimeoutException("No loopback snapshot");
    }
}

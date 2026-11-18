using System;
using System.Collections.Generic;
using Nuclear.Math;

namespace Nuclear.Navigation
{
    /// <summary>Four-neighbor navigation on the XZ plane. Each cell can be marked walkable from level data or physics queries.</summary>
    public sealed class NavigationGrid
    {
        private readonly bool[] blocked;
        public int Width { get; }
        public int Depth { get; }
        public float CellSize { get; }
        public Vector3 Origin { get; }

        public NavigationGrid(int width, int depth, float cellSize, Vector3 origin)
        {
            if (width <= 0 || depth <= 0 || !float.IsFinite(cellSize) || cellSize <= 0)
                throw new ArgumentOutOfRangeException(nameof(cellSize));
            Width = width;
            Depth = depth;
            CellSize = cellSize;
            Origin = origin;
            blocked = new bool[checked(width * depth)];
        }

        private int Cell(int x, int z) => (uint)x < (uint)Width && (uint)z < (uint)Depth ? z * Width + x : -1;
        private int Cell(Vector3 point) => Cell((int)MathF.Floor((point.x - Origin.x) / CellSize),
            (int)MathF.Floor((point.z - Origin.z) / CellSize));

        public void SetBlocked(int x, int z, bool value)
        {
            int cell = Cell(x, z);
            if (cell < 0) throw new ArgumentOutOfRangeException(nameof(x));
            blocked[cell] = value;
        }

        /// <summary>Returns cell-center waypoints, including start and goal, or an empty path when unreachable.</summary>
        public IReadOnlyList<Vector3> FindPath(Vector3 start, Vector3 goal)
        {
            int first = Cell(start), last = Cell(goal);
            var path = new List<Vector3>();
            if (first < 0 || last < 0 || blocked[first] || blocked[last]) return path;
            var open = new PriorityQueue<int, int>();
            var parent = new Dictionary<int, int> { [first] = -1 };
            var cost = new Dictionary<int, int> { [first] = 0 };
            open.Enqueue(first, 0);
            while (open.TryDequeue(out int at, out _))
            {
                if (at == last)
                {
                    for (int i = last; i >= 0; i = parent[i])
                        path.Add(new Vector3(Origin.x + (i % Width + .5f) * CellSize, Origin.y,
                            Origin.z + (i / Width + .5f) * CellSize));
                    path.Reverse();
                    break;
                }
                int x = at % Width, z = at / Width;
                void Visit(int nx, int nz)
                {
                    int next = Cell(nx, nz), distance = cost[at] + 1;
                    if (next < 0 || blocked[next] || (cost.TryGetValue(next, out int old) && old <= distance)) return;
                    cost[next] = distance;
                    parent[next] = at;
                    open.Enqueue(next, distance + System.Math.Abs(nx - last % Width) + System.Math.Abs(nz - last / Width));
                }
                Visit(x - 1, z); Visit(x + 1, z); Visit(x, z - 1); Visit(x, z + 1);
            }
            return path;
        }
    }
}

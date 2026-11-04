# Repository knowledge graph

The repository graph is built with [Graphify](https://github.com/Graphify-Labs/graphify), using local AST extraction for code and the coding assistant for document extraction. Generated relationships describe source structure and attributed documentation claims; they do not establish that an engine feature builds or runs.

## Open the graph

- [Interactive graph](../graphify-out/graph.html): search and filter the detected communities. Large graphs use an aggregated community view.
- [Graph report](../graphify-out/GRAPH_REPORT.md): hubs, communities, connections, provenance, and extraction limitations.
- [Graph data](../graphify-out/graph.json): persistent nodes and relationships for Graphify's query tools.

These links require a local Graphify run. The output files remain available for review or sharing; caches and machine-specific sidecars are ignored by Git. No Git hooks or global assistant instructions are installed by building the graph.

## When to use the graph

- For architecture and cross-file dependency questions, query an existing graph first. Start with `--budget 1500` and narrow the query or use explicit node IDs before expanding the output.
- For a simple symbol or filename lookup, use scoped `rg` or [SourceReference.md](SourceReference.md) directly.
- Read the relevant implementation and callers after locating them. Graph relationships and document claims are navigation evidence; current source takes precedence.
- Keep the full `graph.json` and `GRAPH_REPORT.md` out of routine context. Read a scoped query result or a specific report section when needed.
- If relevant graph data is stale, refresh it before relying on it. Code changes use `graphify update .`; changed documents need a semantic pass through the Graphify skill. Refresh instructions are below.
- If the graph is absent, unavailable, or incomplete for the task, continue with scoped source searches and the source reference. State any material uncertainty; do not infer a missing relationship from the graph's silence or require a full rebuild for a small lookup.

## Query and refresh

Run these commands from the repository root. The initial graph used Graphify **0.9.102** and Python **3.12**, isolated through `uv`:

```powershell
uv run --python 3.12 --with graphifyy==0.9.102 graphify query "ModuleManager lifecycle" --seed nuclear_engine_include_core_modulemanager_modulemanager --budget 1500
uv run --python 3.12 --with graphifyy==0.9.102 graphify explain nuclear_engine_include_assets_tasks_textureimporttask_textureimporttask
uv run --python 3.12 --with graphifyy==0.9.102 graphify path nuclear_engine_include_assets_tasks_textureimporttask_textureimporttask nuclear_engine_include_assets_tasks_texturecreatetask_texturecreatetask --undirected
uv run --python 3.12 --with graphifyy==0.9.102 graphify update .
uv run --python 3.12 --with graphifyy==0.9.102 graphify export html
```

`query` returns a source-attributed subgraph. Use a full node ID or source path when short symbol names are ambiguous. An undirected path establishes connectivity, not a directed call chain. The default graph uses undirected storage for exploration; read each relationship's source evidence before interpreting its direction.

`update` refreshes structural code extraction and retains existing document knowledge; document changes need another semantic pass through the Graphify skill. To build a fresh graph without document semantics, use `graphify extract . --code-only`; to rebuild the complete mixed corpus, invoke the Graphify skill on `.` in a coding assistant. The skill can use the current assistant session for documents without a separate API key. Installing Graphify globally is optional; the `uv run` commands also work in a new session.

## Scope and interpretation

[.graphifyignore](../.graphifyignore) excludes external SDKs, bundled libraries, build outputs, and sample media. The graph covers supported source files in the engine, editor, plugins, managed bridge, ScriptCore, samples, and repository documents. The included Nuclear.Managed fork is indexed because its host and interop code are maintained here.

Graphify does not index every repository format: native `.vcxproj`, `.NuclearShader`, and HLSL files are not parsed by this run. Consult [SourceReference.md](SourceReference.md) and the actual project/shader files for those boundaries. C++ macros and templates can cause parser recovery; the generated report records the files affected. Unresolved references and multiple relationships between the same endpoints can also limit graph completeness.

Edges carry `EXTRACTED`, `INFERRED`, or `AMBIGUOUS` confidence and source provenance. An `EXTRACTED` edge from a document means the document makes that claim; consult current code before treating it as implemented behavior. In particular, `NuclearEngine_Modular_Architecture.md` is a proposal, and its concepts are labeled as proposed rather than current architecture. Older guides retain their original attribution.

For build or runtime validation, follow [BuildAndValidation.md](BuildAndValidation.md). Graph generation and query checks do not compile or launch the engine.

# PageRank (C++23, HPC-oriented)

A modern C++ implementation of PageRank, designed as a platform for studying
shared-memory performance: a correct sequential reference first, then
parallel and cache-aware variants measured against it.

> The original 2022 C implementation (pthreads, producer/consumer parsing)
> lives in the `c-legacy` branch and is kept for historical reference only.

---

## 1. The PageRank algorithm

### 1.1 Model

Given a directed graph `G = (V, E)` with `N = |V|` nodes, PageRank assigns to
each node `j` a score `x_j` that is the stationary distribution of a random
surfer who, at each step:

- with probability `d` (the *damping factor*) follows a uniformly random
  outgoing arc of the current node;
- with probability `1 - d` jumps to a uniformly random node (*teleport*).

Notation:

- `IN(j)  = { i : (i, j) ∈ E }`: nodes pointing to `j`
- `out(i) = |{ j : (i, j) ∈ E }|`: out-degree of `i`
- *dead-end*: a node with `out(i) = 0`

### 1.2 Dead-end nodes

A surfer on a dead-end has no arc to follow. The standard fix, used here, is
to treat a dead-end as if it linked to **every** node uniformly. This keeps
the total rank equal to 1 at every iteration.

### 1.3 Iteration

Starting from `x⁰_j = 1/N`, each iteration computes (pull formulation):

```
x^{t+1}_j = (1 - d)/N  +  d * ( Σ_{i ∈ IN(j)} x^t_i / out(i)  +  (1/N) Σ_{i : out(i)=0} x^t_i )
```

- First term: teleport.
- Second term: rank received through real arcs.
- Third term: rank redistributed from dead-ends. It is a **single scalar**
  per iteration, independent of `j`, so it is computed once and added to every
  component.

The pull form writes only to `x^{t+1}_j`, so different `j` never conflict.
This is what makes the main loop trivially parallel over `j`.

### 1.4 Convergence

The iteration stops at the first `t` such that

```
‖x^{t+1} − x^t‖₁ = Σ_j |x^{t+1}_j − x^t_j|  <  ε
```

or when `maxiter` iterations have been performed (in which case the program
reports *"Did not converge"*).

Invariant used in tests: `Σ_j x_j = 1` at every iteration (up to
floating-point error).

### 1.5 Preprocessing of the input graph

Before iterating, the input is normalised:

- **self-loops** `(i, i)` are discarded;
- **duplicate arcs** are collapsed into one;
- `out(i)` is computed on the arcs that survive.

The number of discarded arcs is `declared_edges − valid_edges` and is part of
the program output.

### 1.6 Reference cost

Per iteration: `O(N + M)` operations, with `M` valid arcs. The kernel is
**memory-bound**: for every arc it reads one source index (sequential) and one
rank contribution (random access). This is the reason the data layout below
matters more than raw arithmetic.

---

## 2. Data structures

### 2.1 Overview

```
 .mtx file ──parse──► EdgeList ──build──► Graph (CSR, transposed, immutable)
                                              │
                                              ▼
                                       PageRank kernel
                                  (x, x_next, contrib vectors)
```

Parsing and graph construction are separate stages with a plain data type in
between, so each can be replaced or parallelised independently.

### 2.2 Types

| Alias     | Underlying | Rationale                                                     |
|-----------|------------|---------------------------------------------------------------|
| `NodeId`  | `uint32_t` | Halves index traffic w.r.t. `size_t`. Limit: `N < 2³²`.       |
| `EdgeIdx` | `uint64_t` | Offsets must address `M` arcs, which may exceed `2³²`.        |

Node ids are **0-based internally**. The Matrix Market format is 1-based; the
conversion (and its bounds check) happens in the parser and nowhere else.

### 2.3 `EdgeList` (parse output)

```cpp
struct Edge     { NodeId src, dst; };
struct EdgeList {
    NodeId            n_nodes;
    std::uint64_t     declared_edges;   // value in the file header
    std::vector<Edge> edges;            // as read: may contain loops/duplicates
};
```

Array-of-structs on purpose: this is a build-phase container, not the hot
loop.

### 2.4 `Graph` (CSR over the transposed graph)

The pull iteration needs fast access to **incoming** arcs, so the graph is
stored in Compressed Sparse Row form over incoming neighbours:

| Array          | Size    | Meaning                                                   |
|----------------|---------|-----------------------------------------------------------|
| `offsets`      | `N + 1` | arcs entering `j` are `sources[offsets[j] .. offsets[j+1])` |
| `sources`      | `M`     | source node of each arc, grouped by destination           |
| `out_degree`   | `N`     | `out(i)`, used for `1/out(i)` and dead-end detection      |

Invariants (checked in debug builds and by unit tests):

- `offsets[0] == 0`, `offsets[N] == M`, `offsets` non-decreasing;
- within each row, `sources` is sorted ascending with no duplicates;
- no `sources[e] == j` for `e` in row `j` (no self-loops);
- `out_degree[i] == count of i in sources`.

The `Graph` is **immutable** after construction: there is no public
`add_edge`. It is built only via `Graph::from_edges(const EdgeList&)`.

Approximate footprint: `8(N+1) + 4M + 4N` bytes.

### 2.5 Construction algorithm

1. Count incoming arcs per destination.
2. Exclusive prefix sum → `offsets`.
3. Scatter each `src` into its row.
4. Per row: sort, remove duplicates and self-loops, compact.
5. Recompute `offsets` and `out_degree` from the surviving arcs.

A simpler first version is also acceptable: pack each arc as
`(uint64_t(dst) << 32) | src`, sort globally, `unique`, and derive the arrays
in one pass (`O(M log M)`). Both sit behind the same interface and can be
benchmarked against each other.

### 2.6 PageRank working vectors

| Vector     | Type     | Purpose                                                       |
|------------|----------|---------------------------------------------------------------|
| `x`        | `double` | rank at iteration `t`                                         |
| `x_next`   | `double` | rank at iteration `t+1`                                       |
| `contrib`  | `double` | `x[i] / out(i)`, `0` for dead-ends, recomputed every iteration |

Precomputing `contrib` removes the division and one random access (`out[i]`)
from the per-arc inner loop. Each iteration is therefore:

1. `contrib[i] = x[i] * inv_out[i]` for all `i`, and the dead-end mass
   `S = Σ_{out(i)=0} x[i]`;
2. `x_next[j] = (1-d)/N + d * ( Σ_{i ∈ IN(j)} contrib[i] + S/N )`, accumulating
   `‖x_next − x‖₁` in the same loop;
3. swap `x` and `x_next`; test convergence.

---

## 3. Command-line interface

```
pagerank [options] <graph.mtx>
```

### 3.1 Positional argument

`<graph.mtx>` (required): Matrix Market coordinate file.

- leading lines starting with `%` are comments;
- first non-comment line: `R C M` (rows, columns, number of arcs). The graph
  is square, so `R` must equal `C`, and `N = R`;
- then `M` lines `i j`, meaning an arc `i → j`, with `1 ≤ i, j ≤ N`.

The file is validated strictly: a malformed line, an index out of range, or a
number of arcs different from the declared `M` is an error.

### 3.2 Options

| Short | Long          | Type   | Default | Valid range              | Meaning                                    |
|-------|---------------|--------|---------|--------------------------|--------------------------------------------|
| `-d`  | `--damping`   | double | `0.9`   | `0 < d < 1`              | damping factor                             |
| `-e`  | `--eps`       | double | `1e-7`  | finite, `> 0`            | convergence threshold on the L1 error      |
| `-m`  | `--maxiter`   | int    | `100`   | `≥ 1`                    | maximum number of iterations               |
| `-k`  | `--top`       | int    | `3`     | `≥ 1` (clamped to `N`)   | number of top-ranked nodes to print        |
| `-t`  | `--threads`   | int    | `1`     | `≥ 1`                    | worker threads (ignored by sequential backend) |
| `-h`  | `--help`      | flag   |         |                          | print usage and exit                       |

Parsing rules:

- numbers are parsed with `std::from_chars`; trailing characters (`-k 5x`),
  empty values, overflow, `NaN` and `inf` are rejected;
- `d = 1` is rejected: with no teleport term, convergence is not guaranteed;
- `-k` larger than `N` is clamped to `N` once the graph is loaded;
- unknown options and a missing file are errors.

Errors are reported on `stderr` with a non-zero exit code; library code never
calls `exit()` (errors are propagated as `std::expected`).

| Exit code | Meaning                         |
|-----------|---------------------------------|
| `0`       | success (converged or not)      |
| `1`       | invalid command-line arguments  |
| `2`       | I/O error or malformed input    |

### 3.3 Output

Written to `stdout`:

```
Number of nodes: 9
Number of dead-end nodes: 2
Number of valid arcs: 11
Converged after 41 iterations
Sum of ranks: 1.0000   (should be 1)
Top 5 nodes:
  5 0.242186
  3 0.211610
  2 0.167547
  1 0.104444
  7 0.102626
```

- node ids in the output are **0-based**;
- if the limit is hit, the convergence line is
  `Did not converge after <maxiter> iterations`;
- the format is kept identical to the legacy program so that `9nodi.sol` acts
  as a regression oracle:

```
./pagerank -e 1e-9 -k 5 tests/data/9nodi.mtx | diff -bB - tests/data/9nodi.sol
```

### 3.4 Example

```
./pagerank -d 0.85 -e 1e-9 -m 200 -k 10 web-Google.mtx
```

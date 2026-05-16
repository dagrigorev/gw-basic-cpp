# Example BASIC Programs

Run examples from the repository root:

```bash
gwbasic --file ./examples/snake.bas
gwbasic --headless --file ./examples/sincos.bas
gwbasic --check --file ./examples/raycast.bas
```

## Catalog

| Program | Purpose |
| --- | --- |
| `circles.bas` | Simple graphics primitives and color changes. |
| `commander.bas` | Text/graphics UI inspired by classic file managers. |
| `easter.bas` | Decorative graphics demo with drawing commands. |
| `pi_pixel.bas` | Estimates pi by counting pixels inside a quarter circle. |
| `raycast.bas` | Larger real-time graphics demo. |
| `sincos.bas` | Plots sine and cosine curves. |
| `snake.bas` | Keyboard-driven game using `INKEY$` and graphics. |
| `tan_graph.bas` | Dense mathematical plot for graphics throughput. |

Use `--headless` when running in CI or when a graphics window is not desired.

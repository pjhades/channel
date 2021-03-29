# Channel

Go-like channels based on futex but still WIP.

Inspired by the following gems:

- https://akkadia.org/drepper/futex.pdf
- https://docs.google.com/document/d/1yIAYmbvL3JxOKOjuCyon7JhW4cSv1wy5hC0ApeGMV9s/pub
- https://github.com/tylertreat/chan

TODOs:

- Benchmark
- Cache line paddings
- Review the memory ordering constraints
- Maybe test on Raspberry Pi (ARM)
- Maybe implement `select`

To build, you need ninja and `ninja_syntax` Python module.

```bash
$ ./configure.py
$ ninja
```

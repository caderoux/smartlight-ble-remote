# Third-party notices

## ha-ble-adv
`src/agarce.h` and `src/agarce.cpp` are a C++ port of the `agarce` codec in
[NicoIIT/ha-ble-adv](https://github.com/NicoIIT/ha-ble-adv) (`custom_components/ble_adv/codecs/agarce.py`), corrected
and extended as described in `docs/protocol.md`. Its test vectors are used in `test/test_agarce`, along with one group
command captured from the app and published by the same author in
[NicoIIT/esphome-components issue 17](https://github.com/NicoIIT/esphome-components/issues/17); the group command's
layout also follows the author's gist `b3acaa75`. ha-ble-adv is licensed as follows:

```
MIT License

Copyright (c) 2019 - 2025  @NicoIIT

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Libraries the example uses (not included here; PlatformIO fetches them)
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino) (Apache-2.0)
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) (its own licence, mostly FreeBSD)
- [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) (an MIT-style licence, in its source)

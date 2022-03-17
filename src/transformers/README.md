# Transformers

Transformer `a2b` is a function that takes a single `const A &a` argument and return an instance of type `B b`
representing `a` in some (intentionally fuzzy) way.

## Bijective transformers

_If a transformer function `a2b` is defined, mapping instances of type `A` to instances of type `B` and a
transformer `b2a` is defined, mapping from `B` to `A`, and if `a == b2a(a2b(a, b))`_, then `b2a` is the inverse of `a2b`
, and these two functions define a _bijection_ between `A` and `B`.

Function pairs like this, or a single _lossless_ function for which defining a 1:1 inverse transformation seems
realistic, should go in the `bijective` directory.

A function `a2b` without its inverse to go along with it should only go in the `bijective` directory if the mapping
from `a` to `b` is _lossless_. That is, the result `b` should contain all the information to (realistically) define
a `b2a` function to map `a` back to itself, exactly. Following this constraint means if we later want to define a new
function `b2a` meeting the same criteria, and put it in `bijective`, we can be more confident that we've maintained the
bijectivity contract of this directory.

**New functions added to `bijective` should always unit-test this assumption!**

## Lossy transformers

Lossy transformations can't, by definition, be bijective, and they should go in the `lossy` directory.

## Examples

Here are some examples of relationships that make sense as a transformer:

### Bijective examples

* `b` is some transformation of `a`
* `b` maps 1:1 to `a`
* `b` is a different format of `a`
* `b` is an encoding of `a` (either lossless or lossy)
* `b` is derived from `a`

### Lossy examples

* `b` looks and sounds basically like `a`
* `b` is a rough translation of `a`
* `a2a` produces some weird nonlinear transformation of a signal with type `a`
* `wav2letter` takes an audio signal and produces one possible stream of letters corresponding to the (hopefully vocal)
  audio

## Conventions

### Naming

The `a2b` naming convention for transformers comes from [`word2vec`](https://arxiv.org/abs/1301.3781), and the lineage
of papers ([wav2vec](https://arxiv.org/abs/1904.05862)
, [wav2letter](https://github.com/flashlight/wav2letter), [vid2vid](https://github.com/NVIDIA/vid2vid), ...) since that
have adopted this cheeky naming scheme.

### Naming lossy transformations and their types

If we want to define a _lossy transformation_ from `Apple`s to `Banana`s (one for which, we have good reason to believe,
there _does not exist_ any function that could transform the produced `banana` back into the original `apple`), it might
make sense to create a new type, e.g. `MushyBanana`, and call our transformer `Apple2MushyBanana`.

The idea with this approach is that someone reading the function name `MushyBanana2Apple` will likely not expect it to
take a mushy banana `b` and transform it back into some pristine apple in all meaningful ways identical to the apple
that a transformer named `Apple2MushyBanana` processed.

# bloom

![License](https://img.shields.io/github/license/droberson/bloom)
![GitHub Issues](https://img.shields.io/github/issues/droberson/bloom)


This project includes implementations of various probabilistic data
structures and algorithms in C.

## Classic, vanilla bloom filters

Bloom filters are space-efficient, probabilistic data structures that
provide a mechanism to quickly determine if an element is _likely_ a
member or _definitely not_ a member of a large dataset.

False positives are possible with bloom filters, but false negatives
are not. Bloom filters can be used to represent large datasets in a
small amount of space, with fast lookups and insertions of elements.

Bloom filters have been used to implement caches, spell checkers, and
more.

This blog post gives a great explanation with visual aids to
understand these data structures: https://samwho.dev/bloom-filters/

https://en.wikipedia.org/wiki/Bloom_filter


## Time-decaying bloom filters

Time-decaying bloom filters are bloom filters with a time
condition. By representing elements with timestamps rather than bits,
a developer can use time-decaying bloom filters to determine when an
element was added to a dataset and make decisions or answer questions
such as:

"If I have seen this data in the last N seconds, do this. Otherwise,
do that."

"Is this element in the set? If so, when was it added?"

## Counting bloom filters

These are like bloom filters, but rather than storing binary bits to
represent an element, a counter is used. This provides the developer
with the ability to remove elements from a set at the expense of
larger filter sizes.

At the moment, this uses a `uint8_t` (8 bits, maximum count 255) as
counters. If larger counters are needed, this can be modified to use
`uint16_t` to increase counter capacity to 65535, however this doubles
the memory and storage required to represent a filter. `uint32_t`
would provide even larger counters, but at quadruple the size.

## Cuckoo filters

Cuckoo filters are a similar concept to bloom filters, but implemented
with a different strategy. In some cases, they may be more
space-efficient or performant than a bloom filter. Cuckoo filters also
support deletion, whereas bloom filters do not.

## Naive Bayes

Naive Bayes can be used to "classify" data using probability
techniques. Example use cases of a Naive Bayes classifier would be
using it to determine (classify) if an email is spam or not spam, or
if a file is or isn't malicious.

Naive Bayes can be used on streaming data with with Gaussian
distribution.

https://en.wikipedia.org/wiki/Naive_Bayes_classifier
https://en.wikipedia.org/wiki/Statistical_classification
https://en.wikipedia.org/wiki/Normal_distribution

### Mahalanobis distance

Malalanobis distance can be used in conjunction with Naive Bayes to
measure how different (distant) an item is from a class. This can be
used in anomaly detection applications.

https://en.wikipedia.org/wiki/Mahalanobis_distance

# Building

This is developed on Debian Linux, and untested on anything else.

Build this with CMake:

Make a directory;
```
mkdir build && cd build
```

Run cmake:
```
cmake ..
```

Build:
```
make
```

Install:
```
make install
```

You may need to run `ldconfig` after installing.


# Testing

running `make test` from the build directory should run unit tests.

# About

I have been interested in probabilistic data structures for several
years. I've used these techniques in various applications, but I
usually code a bespoke implementation specific to my particular
application, written in ways that are non-conductive to reuse.

I am not a mathematician, computer scientist, or professional
developer, but I've tried to make this library generic and easy to use
in a variety of settings.

This library began as a simple bloom filter and time-decaying bloom
filter implementation that I wrote a while ago and thew up on GitHub
in case I needed to use it later. A project came up that I wanted to
use some probabilistic techniques, so I revisited this and started
adding to it. It would be worth my time to make it into a library,
make it more robust, and support additional algorithms.

Since I am not a mathematician, learning how to implement these was
painful because I didn't understand a lot of the verbiage I read in
books, writeups, white papers, etc, when initially attempting to
implement these. In the end, most of these algorithms are simple, but
took significant effort for me to understand. Hopefully, these
examples can help others make more sense of these algorithms and be
leveraged for good.


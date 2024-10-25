# archbloom

![License](https://img.shields.io/github/license/droberson/bloom)
![GitHub Issues](https://img.shields.io/github/issues/droberson/bloom)


This project includes implementations of various probabilistic data
structures and algorithms in C, presented as a library.

## Classic, vanilla bloom filters

Bloom filters are space-efficient, probabilistic data structures that
provide a mechanism to quickly determine if an element is _likely_ a
member or _definitely not_ a member of a large dataset.

False positives are possible with bloom filters, but false negatives
are not. Bloom filters can be used to represent large datasets in a
small amount of space, with fast lookups and insertions of elements.

Bloom filters have been used to implement databases, caches, word
hyphenation algorithms and spell checkers, and more.

The Bloom filter principle: Wherever a list or set is used, and space
is at a premium, consider using a Bloom filter if the effect of false
positives can be mitigated. -- Broder & Mitzenmacher

This blog post gives a great explanation with visual aids to
understand these data structures: https://samwho.dev/bloom-filters/

The Wikipedia article for Bloom filters was helpful to me for initial
understanding and implementation: https://en.wikipedia.org/wiki/Bloom_filter

The original paper by Burton H. Bloom, written in 1970, titled
"Space/Time Trade-offs in Hash Coding with Allowable Errors" can be
found here: https://dl.acm.org/doi/pdf/10.1145/362686.362692

"Network Applications of Bloom Filters: A Survey" by Andrei Broder and
Michael Mitzenmacher was also instrumental to helping me understand
these data structures and the problems they can be used to solve:
https://www.eecs.harvard.edu/~michaelm/postscripts/im2005b.pdf

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

This implementation supports using 1, 2, 4, and 8 byte counters. This
can reduce the memory footprint required for counting bloom filters at
the expense of lower maximum value for the counters. If an application
doesn't expect to have large values in a counting bloom filter, using
a smaller width counter will reduce memory costs.

## Time-decaying, counting Bloom filters

Time-decaying, counting Bloom filters combine the properties of
time-decaying and counting Bloom filters. This allows the filter to
not only track the presence of an element but also how many times the
element has been seen within a specified time window.

For example, these filters can answer questions such as: "How many
times have I seen this element in the last thirty minutes?" or "Has
this element's occurrence rate increased over time?" or "Has this
element exceeded a threshold in the last N minutes?"

## Count-Min Sketch

NOT IMPLEMENTED YET.

Count-Min Sketch is used for approximating frequency of elements and
suitable for use on streaming data. Count-Min Sketch can answer how
many times an element has been seen, with the potential of slight
overcounting, but never undercounting.

These can be used to answer questions such as "How many times has this
domain name been seen?"

## Spectral Bloom Filters

NOT IMPLEMENTED YET.

Spectral Bloom Filters are designed to determine multi-set membership
and approximate counts of elements within several sets. These allow
for estimation of frequency or multiplicity of elements rather than
existence or count as provided by normal Bloom filters and counting
Bloom filters.

These are remarkably similar to counting Bloom filters, but have
different logic for updating counters, querying the filter,
removing/decreasing elements, and methods of managing collisions.

## Cuckoo filters

PARTIALLY IMPLEMENTED.

Cuckoo filters are a similar concept to bloom filters, but implemented
with a different strategy. In some cases, they may be more
space-efficient or performant than a bloom filter. Cuckoo filters also
support deletion, whereas bloom filters do not.

## Naive Bayes

PARTIALLY IMPLEMENTED.

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

# Doxygen

To build documentation with Doxygen, please be sure that `doxygen` and
`graphviz` are installed on your system:

```
apt install doxygen graphviz
```

Then invoke cmake, defining DBUILD_DOC=ON:

```
cmake -DBUILD_DOC=ON
```

# Testing

Running `make test` from the build directory should run unit tests.

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

This was named "archbloom" during a conversation with my big homie
Quinten. I needed to rename the project because "libbloom" is already
taken and this does a lot more than bloom filters now. Quinten
mentioned the name "archbloom", so I Googled this to see if it had any
jacked up connotations that I wasn't aware of. One of the first
results was someone's World of Warcraft character profile. This
character was a "night elf feral druid". Knowing next to nothing about
WoW, I thought this was pretty hard. As such, archbloom has been
written into lore and it simply is what it is.

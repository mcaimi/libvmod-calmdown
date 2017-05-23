# vmod-calmdown

## SYNOPSIS

    import calmdown;

## DESCRIPTION

A simple rate-limit module for Varnish Cache.

This module implements a rate limiting feature in Varnish that allows an user to set
limits on how often a resource should be requested by the same source client IP. This is
useful for example for internet-facing API providers, high-traffic services or websites 
in general.

The algorithm used is a variant of the [Token-Bucket algorithm](https://en.wikipedia.org/wiki/Token_bucket)

The module works by:
1. instantiating an array of linked lists (the bucket queues) and associating a pthread mutex to every item
2. for every request the module computes the SHA1 hash of the source IP and determine which queue will handle the request
3. once a queue is selected, a bucket is allocated for the current source IP hash.

For every request, the time between the last call timestamp and the current is evaluated and the token counter is:
1. Decreased by one
2. Increased by a value proportional to the timestamp difference.

If a source IP hash consumes all its tokens, the user receives a "calm down" error from varnish.

## FUNCTIONS

    *calmdown()*

### Prototype:

    calmdown(STRING S, INT I, DURATION D)

### Return value:

BOOL

### Description

  Rate limits access to resources.

  S: is the actual URL you want to rate limit
  I: is the initial token (number of calls) number associated to every bucket
  D: the minimum interval in which varnish will let pass at most 'I' calls.

### Example

    sub vcl_recv {
      if (calmdown.calmdown(client.identity, 15, 10s)) {
      # Client has exceeded 15 reqs per 10s
      return (synth(429, "Calm Down"));
    }

## INSTALLATION

The source tree is based on autotools to configure the building.
Building requires the Varnish header files and uses pkg-config to find
the necessary paths.

Usage:

    ./autogen.sh
    ./configure

If you have installed Varnish to a non-standard directory, call
``autogen.sh`` and ``configure`` with ``PKG_CONFIG_PATH`` pointing to
the appropriate path. For instance, when varnishd configure was called
with ``--prefix=$PREFIX``, use

    export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
    export ACLOCAL_PATH=${PREFIX}/share/aclocal

The module will inherit its prefix from Varnish, unless you specify a
different ``--prefix`` when running the ``configure`` script for this
module.

Make targets:

* make - builds the vmod.
* make install - installs your vmod.

In addition to these steps, you need to install the config YAML file:

* copy src/conf/settings.yaml into /etc/vmod-calmdown/calmdown.yaml

as of now you *need* to create that specific folder in /etc, as the 
software will search for that hardcoded path.

### Installation directories

By default, the vmod ``configure`` script installs the built vmod in the
directory relevant to the prefix. The vmod installation directory can be
overridden by passing the ``vmoddir`` variable to ``make install``.

## USAGE

In your VCL you could then use this vmod along the following lines::

    import calmdown;

    sub vcl_recv {
      if (calmdown.calmdown(client.identity, 15, 10s)) {
      # Client has exceeded 15 reqs per 10s
      return (synth(429, "Calm Down"));
    }

## COMMON PROBLEMS

* configure: error: Need varnish.m4 -- see README.rst

  Check whether ``PKG_CONFIG_PATH`` and ``ACLOCAL_PATH`` were set correctly
  before calling ``autogen.sh`` and ``configure``

* Incompatibilities with different Varnish Cache versions

  Make sure you build this vmod against its correspondent Varnish Cache version.
  For instance, to build against Varnish Cache 5.1, this vmod must be built from
  branch 5.1.

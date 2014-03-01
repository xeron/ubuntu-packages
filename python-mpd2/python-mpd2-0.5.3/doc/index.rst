python-mpd2 Documentation
=========================

*python-mpd2* is a Python library which provides a client interface for
the `Music Player Daemon <http://musicpd.org>`__.

Difference with python-mpd
--------------------------

python-mpd2 is a fork of `python-mpd`_.  While 0.4.x was backwards compatible
with python-mpd, starting with 0.5 provides enhanced features which are *NOT*
backward compatibles with the original `python-mpd`_ package.  See
:doc:`Porting <topics/porting>` for more information.

The following features were added:

-  Python 3 support (but you neead at least Python 2.6)
-  support for the upcoming client-to-client protocol
-  support for new commands from MPD v0.17 (seekcur, prio, prioid,
   config, searchadd, searchaddpl) and MPD v0.18 (readcomments, toggleoutput)
-  remove deprecated commands (volume)
-  explicitly declared MPD commands (which is handy when using for
   example `IPython <http://ipython.org>`__)
-  a test suite
-  API documentation to add new commands (see :doc:`Future Compatible <topics/advanced>`)
-  support for Unicode strings in all commands (optionally in python2,
   default in python3 - see :doc:`Unicode Handling <topics/advanced>`)
-  configureable timeouts
-  support for :doc:`logging <topics/logging>`
-  improved support for sticker
-  improved support for ranges

If you like this module, you could try contact the original author
jat@spatialrift.net or join the discussion on the 
`issue tracker <http://jatreuman.indefero.net/p/python-mpd/issues/7/>`__ so that
it gets merged upstream.

Getting Started
===============

A quick guide for getting started python-mpd2:

* :doc:`Getting Started <topics/getting-started>`

.. _python-mpd: http://jatreuman.indefero.net/p/python-mpd/

Command Reference
=================

A complete list of all available commands:

* :doc:`Commands <topics/commands>`

Changelog
=========

* :doc:`Change log <changes>`

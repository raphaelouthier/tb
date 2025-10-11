
The storage library stores historical trading 
data of different levels for instruments.

The storage system is single-threaded,
but is designed to allow multiple instances
to operate in parallel on disk data.

Multiple readers and writers are supported,
with a theoretical limit of at most one writer
per segment. In practice, this restriction is
extended to at most one writer per level of data
of a given instrument. 

It relies on the segment library to store data,
and handles the organization of those segments
in the file system.

A storage instance operates (in coordination with
other potential instances) on a storage directory
structured as follows :
- (root)/ : root directory.
  - stg : flag file reporting storage directory.
    Content unused.
  - (mkp)/ : marketplace directory.
    - (ist)/ : instrument directory.
      - (level)/ : level directory.
        - idx : index file : synchronizes readers
          and writers, indexes time and segments.
        - (sgm_id) : data segment, described in the index

Levels 1 2 3 (as described in the design doc) are
supported. Level 0 may be supported in the future.

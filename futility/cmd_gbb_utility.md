% FUTILITY(1) Version 1.0 | Futility GBB Documentation

NAME
====

**futility gbb** - allows for the printing and manipulation of GBB flag state.

SYNOPSIS
========

| **futility gbb** \[**\--help**]
| **futility gbb** \[**-g**|**--get**] \[GET mode options] \[image_file] \[output_file]
| **futility gbb** \[**-s**|**--set**] \[SET mode options]
| **futility gbb** \[**-c**|**--create**] \[CREATE mode options]
| **futility gbb** \[**-g**|**-s**] \[**\--flash**] \[GET|SET mode options] \[FLASH options]

DESCRIPTION
===========

The GBB sub-command allows for the printing and manipulation of
the GBB flag state machine.

Options
-------

\--help

:   Prints brief usage information.

-g, \--get

:   Puts the GBB command into GET mode. (default)

-s, \--set

:   Puts the GBB command into SET mode.

-c, \--create=hwid_size,rootkey_size,bmpfv_size,recoverykey_size

:   Puts the GBB command into CREATE mode. Create a GBB blob by given size list.

GET Mode Options
----------------

Get (read) from image_file or flash, with following options:

\--flash

:   Read from and write to flash, ignore file arguments.

### Report Fields

The following options are available for reporting different types of information
from image_file or flash. The default is returing hwid. There could be multiple
fields to be reported at one time.

\--hwid

:   Report hardware id (default).

```
hardware_id: EXAMPLE
```

\--flags

:   Report header flags.

```
flags: 0x00000000
```

\--digest

:  Report digest of hwid (>= v1.2)

```
digest: HASH_CODE
```

-e, \--explicit

:   Report header flags by name. This implies **\--flags**.

```
flags: 0x00000000
VB2_GBB_FLAG_FLAG_A
VB2_GBB_FLAG_FLAG_B
```

### File Names to Export

-k, \--rootkey=FILE

:   File name to export Root Key.

-b, \--bmpfv=FILE

:   File name to export Bitmap FV.

-r, \--recoverykey=FILE

:   File name to export Recovery Key.

SET Mode Options
-------

CREATE Mode Options
-------

FLASH Mode Options
-------

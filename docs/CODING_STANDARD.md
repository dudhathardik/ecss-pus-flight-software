# Coding standard

A short, enforceable subset of the rules a flight software coding standard
carries, chosen so that each one is either checked by the build or visible in
a five-minute review. Rules referenced from comments in the sources use the
identifiers below.

## General

- **R-GEN-01** The language is C99. No compiler extensions, no VLAs, no
  anonymous unions.
- **R-GEN-02** The flight sources build with `-Wall -Wextra -Wpedantic
  -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Werror`.
  A warning is a finding, not a note.
- **R-GEN-03** One module is one `.c` file plus one `.h` file of the same
  name. Anything not declared in the header is `static`.
- **R-GEN-04** No `goto`, no recursion, no function pointers in the flight
  sources. Every loop has a bound that is visible at the loop head.

## Memory

- **R-MEM-01** No dynamic memory allocation. Every buffer is a compile-time
  sized static or automatic object, and every size comes from
  `include/pus/pus_config.h`. Verified by `make static`.
- **R-MEM-02** Every function that writes into a caller's buffer takes the
  capacity of that buffer as an argument and returns
  `PUS_ERR_BUFFER_TOO_SMALL` rather than truncating or overrunning.
- **R-MEM-03** No pointer arithmetic beyond indexing a buffer whose length is
  in scope.

## Types and numerics

- **R-NUM-01** Fixed width types from `<stdint.h>` everywhere. Plain `int`
  appears only as a return type of `main`.
- **R-NUM-02** No floating point in the flight sources. Physical quantities
  are scaled integers, and the scale is in the name or the comment
  (`temperature_dc` is in units of 0.1 degC).
- **R-NUM-03** Every implicit conversion that could change a value is written
  as an explicit cast. `-Wconversion` enforces this.
- **R-NUM-04** Protocol fields are read and written octet by octet through
  `pus_bytes.h`. No struct overlays on received data, no casts to a wider
  type, no assumption about the endianness or the alignment of the target.

## Control flow and error handling

- **R-ERR-01** Every function that can fail returns `pus_status_t`. Status
  codes are never re-used for data.
- **R-ERR-02** A return value is either used or explicitly discarded with
  `(void)`. Discarding is allowed only where the failure is already reported
  through another path, and the reason is in the comment.
- **R-ERR-03** Every pointer argument that may not be null is checked at the
  entry of the function.
- **R-ERR-04** Every `switch` on an enumeration or a protocol field has a
  `default` label. `-Wswitch-default` enforces this.
- **R-ERR-05** A telecommand that is rejected changes no state. Lists are
  validated in full before the first element is applied.

## Naming

- **R-NAM-01** File-scope variables are prefixed `s_`, and are `static`.
- **R-NAM-02** Public symbols carry their module as a prefix: `pus_tm_`,
  `svc03_`, `obc_`, `hal_`.
- **R-NAM-03** Macros are upper case; enumerators carry the enumeration name
  as a prefix.

## Comments

- **R-CMT-01** Every public function has a Doxygen block stating what it does,
  what each argument means, and what it returns.
- **R-CMT-02** Every function that implements a requirement carries an
  `@implements SWREQ-...` tag, and every test case carries `@verifies`. The
  build fails if either link is missing.
- **R-CMT-03** Comments explain why, not what. A comment restating the code it
  sits above is a review finding.

// the code below is based on cproc's ops.h and with some modifications

/**
 * Copyright © 2019-2024 Michael Forney
 * 
 * Permission to use, copy, modify, and/or distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright notice
 * and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
*/

/* arithmetic and bits */
OP(IADD, "add")
OP(ISUB, "sub")
OP(INEG, "neg")
OP(IDIV, "div")
OP(IMUL, "mul")
OP(IREM, "rem")

/* memory */
OP(ISTORES, "stores")
OP(ISTOREL, "storel")
OP(ISTOREW, "storew")
OP(ILOADS, "loads")
OP(ILOADL, "loadl")
OP(ILOADW, "loadw")
OP(IALLOC4, "alloc4")
OP(IALLOC8, "alloc8")

/* comparisons */
OP(ICEQW, "ceqw")
OP(ICNEW, "cnew")
OP(ICSLEW, "cslew")
OP(ICSLTW, "csltw")
OP(ICSGEW, "csgew")
OP(ICSGTW, "csgtw")

OP(ICEQS, "ceqs")
OP(ICNES, "cnes")
OP(ICLES, "cles")
OP(ICLTS, "clts")
OP(ICGES, "cges")
OP(ICGTS, "cgts")

/* conversions */
OP(IEXTSW, "extsw")
OP(ISTOSI, "stosi")
OP(ISWTOF, "swtof")

/* arg, param and call */
OP(IPAR, "par")
OP(IARG, "arg")
OP(ICALL, "call")

OP(ICOPY, "copy")
OP(INOP, "nop")

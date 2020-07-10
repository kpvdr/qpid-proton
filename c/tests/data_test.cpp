/*
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 */

#include "./pn_test.hpp"

#include "core/data.h"

#include <proton/codec.h>
#include <proton/error.h>

#include <cstdarg> // va_start(), va_end()
#include <ctime> // time()
#include <string.h> // memset()

using namespace pn_test;

// Make sure we can grow the capacity of a pn_data_t all the way to the max and
// we stop there.
TEST_CASE("data_grow") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(0));
  int code = 0;
  while (pn_data_size(data) < PNI_NID_MAX && !code) {
    code = pn_data_put_int(data, 1);
  }
  CHECK_THAT(*pn_data_error(data), error_empty());
  CHECK(pn_data_size(data) == PNI_NID_MAX);
  code = pn_data_put_int(data, 1);
  INFO(pn_code(code));
  CHECK(code == PN_OUT_OF_MEMORY);
  CHECK(pn_data_size(data) == PNI_NID_MAX);
}

TEST_CASE("data_multiple") {
  auto_free<pn_data_t, pn_data_free> data(pn_data(1));
  auto_free<pn_data_t, pn_data_free> src(pn_data(1));

  /* NULL data pointer */
  pn_data_fill(data, "M", NULL);
  CHECK("null" == inspect(data));

  /* Empty data object */
  pn_data_clear(data);
  pn_data_fill(data, "M", src.get());
  CHECK("null" == inspect(data));

  /* Empty array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_array(src, false, PN_SYMBOL);
  pn_data_fill(data, "M", src.get());
  CHECK("null" == inspect(data));

  /* Single-element array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_array(src, false, PN_SYMBOL);
  pn_data_enter(src);
  pn_data_put_symbol(src, pn_bytes("foo"));
  pn_data_fill(data, "M", src.get());
  CHECK(":foo" == inspect(data));

  /* Multi-element array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_array(src, false, PN_SYMBOL);
  pn_data_enter(src);
  pn_data_put_symbol(src, pn_bytes("foo"));
  pn_data_put_symbol(src, pn_bytes("bar"));
  pn_data_fill(data, "M", src.get());
  CHECK("@PN_SYMBOL[:foo, :bar]" == inspect(data));

  /* Non-array */
  pn_data_clear(data);
  pn_data_clear(src);
  pn_data_put_symbol(src, pn_bytes("baz"));
  pn_data_fill(data, "M", src.get());
  CHECK(":baz" == inspect(data));

  /* Described list with open frame descriptor */
  pn_data_clear(data);
  pn_data_fill(data, "DL[]", (uint64_t)16);
  CHECK("@open(16) []" == inspect(data));

  /* open frame with some fields */
  pn_data_clear(data);
  pn_data_fill(data, "DL[SSnI]", (uint64_t)16, "container-1", 0, 965);
  CHECK("@open(16) [container-id=\"container-1\", channel-max=965]" == inspect(data));

  /* Map */
  pn_data_clear(data);
  pn_data_fill(data, "{S[iii]SI}", "foo", 1, 987, 3, "bar", 965);
  CHECK("{\"foo\"=[1, 987, 3], \"bar\"=965}" == inspect(data));
}


#define BUFSIZE 1024
static void check_encode_decode(auto_free<pn_data_t, pn_data_free>& src) {
	char buf[BUFSIZE];
	auto_free<pn_data_t, pn_data_free> data(pn_data(1));
	pn_data_clear(data);

	// Encode src array to buf
	int enc_size = pn_data_encode(src, buf, BUFSIZE - 1);
	if (enc_size < 0) {
		FAIL("pn_data_encode() error " << enc_size << ": " << pn_code(enc_size));
	}

	// Decode buf to data
	int dec_size = pn_data_decode(data, buf, BUFSIZE - 1);
	pn_error_t *dec_err = pn_data_error(data);
	if (dec_size < 0) {
		FAIL("pn_data_decode() error " << dec_size << ": " << pn_code(dec_size));
	}

	// Checks
	CHECK(enc_size == dec_size);
	CHECK(inspect(src) == inspect(data));
}

static void check_array(const char *fmt, ...) {
	auto_free<pn_data_t, pn_data_free> src(pn_data(1));
	pn_data_clear(src);

	// Create src array
	va_list ap;
	va_start(ap, fmt);
	pn_data_vfill(src, fmt, ap);
	va_end(ap);

	check_encode_decode(src);
}

TEST_CASE("array_null") {
	check_array("@T[]", PN_NULL);
	check_array("@T[nnn]", PN_NULL);
}

TEST_CASE("array_bool") {
	check_array("@T[]", PN_BOOL);
	check_array("@T[oooo]", PN_BOOL, true, false, false, true);
}

TEST_CASE("array_ubyte") {
	check_array("@T[]", PN_UBYTE);
	check_array("@T[BBBBB]", PN_UBYTE, uint8_t(0), uint8_t(1), uint8_t(0x7f), uint8_t(0x80), uint8_t(0xff));
}

TEST_CASE("array_byte") {
	check_array("@T[]", PN_BYTE);
	check_array("@T[bbbbb]", PN_BYTE, int8_t(-0x80), int8_t(-1), int8_t(0), int8_t(1), int8_t(0x7f));
}

TEST_CASE("array_ushort") {
	check_array("@T[]", PN_USHORT);
	check_array("@T[HHHHH]", PN_USHORT, uint16_t(0), uint16_t(1), uint16_t(0x7fff), uint16_t(0x8000), uint16_t(0xffff));
}

TEST_CASE("array_short") {
	check_array("@T[]", PN_SHORT);
	check_array("@T[hhhhh]", PN_SHORT, int16_t(-0x8000), int16_t(-1), int16_t(0), int16_t(1), int16_t(0x7fff));
}

TEST_CASE("array_uint") {
	check_array("@T[]", PN_UINT);
	check_array("@T[IIIII]", PN_UINT, uint32_t(0), uint32_t(1), uint32_t(0x7fffffff), uint32_t(0x80000000), uint32_t(0xffffffff));
}

TEST_CASE("array_int") {
	check_array("@T[]", PN_INT);
	check_array("@T[iiiii]", PN_INT, int32_t(-0x80000000), int32_t(-1), int32_t(0), int32_t(1), int32_t(0x7fffffff));
}

TEST_CASE("array_char") {
	// TODO: PROTON-2249: This test will pass, but is not checking array contents
	// correctly until this issue is fixed.
	auto_free<pn_data_t, pn_data_free> src(pn_data(1));
	pn_data_clear(src);
	pn_data_put_array(src, false, PN_CHAR);
	pn_data_enter(src);
	pn_data_exit(src);
	check_encode_decode(src);

	pn_data_clear(src);
	pn_data_put_array(src, false, PN_CHAR);
	pn_data_enter(src);
	pn_data_put_char(src, pn_char_t(0));
	pn_data_put_char(src, pn_char_t('5'));
	pn_data_put_char(src, pn_char_t('a'));
	pn_data_put_char(src, pn_char_t('Z'));
	pn_data_put_char(src, pn_char_t(0x7f));
	pn_data_exit(src);
	check_encode_decode(src);
}

TEST_CASE("array_ulong") {
	check_array("@T[]", PN_ULONG);
	check_array("@T[LLLLL]", PN_ULONG, uint64_t(0), uint64_t(1), uint64_t(0x7fffffffffffffff), uint64_t(0x8000000000000000), uint64_t(0xffffffffffffffff));
}

TEST_CASE("array_long") {
	check_array("@T[]", PN_LONG);
	check_array("@T[lllll]", PN_LONG, int64_t(-0x8000000000000000), int64_t(-1), int64_t(0), int64_t(1), int64_t(0x8000000000000000));
}

TEST_CASE("array_timestamp") {
	check_array("@T[]", PN_TIMESTAMP);
	check_array("@T[ttt]", PN_TIMESTAMP, int64_t(0), int64_t(std::time(0)*1000), int64_t(0x123456789abcdef));
}

TEST_CASE("array_float") {
	check_array("@T[]", PN_FLOAT);
	check_array("@T[fff]", PN_FLOAT, float(0.0), float(3.14), float(1.234e56), float(-1.234e-56));
}

TEST_CASE("array_double") {
	check_array("@T[]", PN_DOUBLE);
	check_array("@T[ddd]", PN_DOUBLE, double(0.0), double(3.1416), double(1.234e56), double(-1.234e-56));
}

TEST_CASE("array_decimal32") {
	auto_free<pn_data_t, pn_data_free> src(pn_data(1));
	pn_data_clear(src);
	pn_data_put_array(src, false, PN_DECIMAL32);
	pn_data_enter(src);
	pn_data_exit(src);
	check_encode_decode(src);

	pn_data_clear(src);
	pn_data_put_array(src, false, PN_DECIMAL32);
	pn_data_enter(src);
	pn_data_put_decimal32(src, pn_decimal32_t(0));
	pn_data_put_decimal32(src, pn_decimal32_t(0x01234567));
	pn_data_exit(src);
	check_encode_decode(src);
}

TEST_CASE("array_decimal64") {
	auto_free<pn_data_t, pn_data_free> src(pn_data(1));
	pn_data_clear(src);
	pn_data_put_array(src, false, PN_DECIMAL64);
	pn_data_enter(src);
	pn_data_exit(src);
	check_encode_decode(src);

	pn_data_clear(src);
	pn_data_put_array(src, false, PN_DECIMAL64);
	pn_data_enter(src);
	pn_data_put_decimal64(src, pn_decimal64_t(0));
	pn_data_put_decimal64(src, pn_decimal64_t(0x0123456789abcdef));
	pn_data_exit(src);
	check_encode_decode(src);
}

TEST_CASE("array_decimal128") {
	auto_free<pn_data_t, pn_data_free> src(pn_data(1));
	pn_data_clear(src);
	pn_data_put_array(src, false, PN_DECIMAL128);
	pn_data_enter(src);
	pn_data_exit(src);
	check_encode_decode(src);

	pn_data_clear(src);
	pn_data_put_array(src, false, PN_DECIMAL128);
	pn_data_enter(src);
	pn_decimal128_t d1;
	memset(d1.bytes, 0, sizeof(d1.bytes)); // set to all zeros
	pn_data_put_decimal128(src, d1);
	pn_decimal128_t d2;
	char val[] = {'\x01', '\x23', '\x45', '\x67', '\x89', '\xab', '\xcd', '\xef', '\x01', '\x23', '\x45', '\x67', '\x89', '\xab', '\xcd', '\xef'};
	memcpy(d2.bytes, val, sizeof(val)); // copy value
	pn_data_put_decimal128(src, d2);
	pn_data_exit(src);
	check_encode_decode(src);
}

TEST_CASE("array_binary") {
	check_array("@T[]", PN_BINARY);
	check_array("@T[ZZZZZ]", PN_BINARY, 0, "", 2, "\x00\x00", 4, "\x00\x01\xfe\xff", 8, "abcdefgh", 16, "1234567890123456");
}

TEST_CASE("array_string") {
	check_array("@T[]", PN_STRING);
	// TODO: PROTON-2248: using S and s reversed
	check_array("@T[SSSSS]", PN_STRING, "", "hello", "bye", "abcdefg", "the quick brown fox jumped over the lazy dog 0123456789");
}

TEST_CASE("array_symbol") {
	check_array("@T[]", PN_SYMBOL);
	// TODO: PROTON-2248: using S and s reversed
	check_array("@T[sssss]", PN_SYMBOL, "", "hello", "bye", "abcdefg", "the quick brown fox jumped over the lazy dog 0123456789");
}

TEST_CASE("array_array") {
	check_array("@T[]", PN_ARRAY);
	// TODO: PROTON-2248: using S and s reversed
	check_array("@T[@T[]@T[ooo]@T[ii]@T[nnnn]@T[sss]]", PN_ARRAY, PN_UBYTE, PN_BOOL, false, false, true, PN_INT, -100, 100, PN_NULL, PN_SYMBOL, "aaa", "bbb", "ccc");
}

TEST_CASE("array_list") {
	check_array("@T[]", PN_LIST);
	// TODO: PROTON-2248: using S and s reversed
	// empty list as first array element
	check_array("@T[[][oo][][iii][Sosid]]", PN_LIST, true, false, 1, 2, 3, "hello", false, "world", 43210, 2.565e-56);
	// empty list not as first array element
	check_array("@T[[Sid][oooo][]]", PN_LIST, "aaa", 123, double(3.2415), true, true, false, true);
	// only empty lists
	check_array("@T[[][][][][]]", PN_LIST);
}

TEST_CASE("array_map") {
	check_array("@T[]", PN_MAP);
	// TODO: PROTON-2248: using S and s reversed
	check_array("@T[{}{sS}{}{IhIoIf}{iSiSiSiS}]", PN_MAP, "key", "value", 123, -123, 255, false, 0, 0.25, 0, "zero", 1, "one", 2, "two", 3, "three");
}




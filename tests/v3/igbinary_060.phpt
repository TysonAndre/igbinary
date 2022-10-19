--TEST--
Igbinary_unserialize_header warning
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
function my_error_handler($errno, $errstr) {
	printf("Logged: $errstr\n");
}
error_reporting(E_ALL);
set_error_handler('my_error_handler');

function test_igbinary_unserialize($data) {
	$unserialized = igbinary_unserialize($data);
	if ($unserialized !== null) {
		printf("Expected null, got %s\n", var_export($unserialized, true));
	}
}

test_igbinary_unserialize("a:0:{}");
test_igbinary_unserialize('\\""\\{}');
test_igbinary_unserialize("\x00\x00\x00\x01");
test_igbinary_unserialize("\x00\x00\x00\x03\x00");
test_igbinary_unserialize("\x00\x00\x00\x03\x01");
test_igbinary_unserialize("\x00\x00\x00\xff\x00");
test_igbinary_unserialize("\x02\x00\x00\x00\x00");
--EXPECT--
Logged: igbinary_unserialize_header: unsupported version: "a:0:"..., should begin with a binary version header of "\x00\x00\x00\x01" or "\x00\x00\x00\x02" or "\x00\x00\x00\x03"
Logged: igbinary_unserialize_header: unsupported version: "\\\"\"\\"..., should begin with a binary version header of "\x00\x00\x00\x01" or "\x00\x00\x00\x02" or "\x00\x00\x00\x03"
Logged: igbinary_unserialize_header: expected at least 5 bytes of data, got 4 byte(s)
Logged: igbinary_unserialize_v3_zval: unknown type '00', position 5
Logged: igbinary_unserialize_header: unsupported version: 255, should be 1 or 2 or 3
Logged: igbinary_unserialize_header: unsupported version: 33554432, should be 1 or 2 or 3 (wrong endianness?)

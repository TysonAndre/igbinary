--TEST--
Check for simple string serialization v3
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
if(!extension_loaded('igbinary')) {
	dl('igbinary.' . PHP_SHLIB_SUFFIX);
}

function test($type, $variable) {
	$serialized = igbinary_serialize($variable);
	$unserialized = igbinary_unserialize($serialized);

	echo $type, "\n";
	echo bin2hex($serialized), "\n";
	echo $unserialized === $variable ? 'OK' : 'ERROR';
	echo "\n";
}

test('empty: ""', "");
test('string: "foobar"', "foobar");

/*
 * you can add regression tests for your extension here
 *
 * the output of your test code has to be equal to the
 * text in the --EXPECT-- section below for the tests
 * to pass, differences between the output and the
 * expected text are interpreted as failure
 *
 * see TESTING.md for further information on
 * writing regression tests
 */
?>
--EXPECT--
empty: ""
000000030c
OK
string: "foobar"
000000030e06666f6f626172
OK

--TEST--
Check for array+string serialization
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
	echo substr(bin2hex($serialized), 8), "\n";
	if ($unserialized != $variable) {
		echo 'ERROR, expected: ';
		var_dump($variable);
		echo 'got: ';
		var_dump($unserialized);
	} else {
		echo 'OK';
	}
	echo "\n";
}

test('array("foo", "foo", "foo")', array("foo", "foo", "foo"));
test('array("one" => 1, "two" => 2))', array("one" => 1, "two" => 2));
test('array("kek" => "lol", "lol" => "kek")', array("kek" => "lol", "lol" => "kek"));
test('array("" => "empty")', array("" => "empty"));

?>
--EXPECT--
array("foo", "foo", "foo")
190304000e03666f6f0401120004021200
OK
array("one" => 1, "two" => 2))
19020e036f6e6504010e0374776f0402
OK
array("kek" => "lol", "lol" => "kek")
19020e036b656b0e036c6f6c12011200
OK
array("" => "empty")
19010c0e05656d707479
OK

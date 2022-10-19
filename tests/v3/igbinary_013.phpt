--TEST--
Object-Array test
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
if(!extension_loaded('igbinary')) {
	dl('igbinary.' . PHP_SHLIB_SUFFIX);
}

function test($type, $variable, $test) {
	$serialized = igbinary_serialize($variable);
	$unserialized = igbinary_unserialize($serialized);

	echo $type, "\n";
	echo substr(bin2hex($serialized), 8), "\n";
	echo $test || $unserialized == $variable ? 'OK' : 'ERROR';
	echo "\n";
}

class Obj {
	var $a;
	var $b;

	function __construct($a, $b) {
		$this->a = $a;
		$this->b = $b;
	}
}

$o = array(new Obj(1, 2), new Obj(3, 4));


test('object', $o, false);

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
object
190204001c034f626a19020e016104010e016204020401150019021201040312020404
OK

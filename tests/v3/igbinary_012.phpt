--TEST--
Object test
--INI--
; Note that php 8.1 deprecates using Serializable without __serialize/__unserialize but we are testing Serialize for igbinary. Suppress deprecations.
error_reporting=E_ALL & ~E_DEPRECATED
igbinary.use_v3_serialize_format=1
--FILE--
<?php
if(!extension_loaded('igbinary')) {
	dl('igbinary.' . PHP_SHLIB_SUFFIX);
}

function test($type, $variable) {
	$serialized = igbinary_serialize($variable);
	$unserialized = igbinary_unserialize($serialized);
//	$serialized = serialize($variable);
//	$unserialized = unserialize($serialized);

	echo $type, "\n";
	echo substr(bin2hex($serialized), 8), "\n";
//	echo $serialized, "\n";
	echo $unserialized == $variable ? 'OK' : 'ERROR';
	echo "\n";
}

class Obj {
	public $a;
	protected $b;
	private $c;

	function __construct($a, $b, $c) {
		$this->a = $a;
		$this->b = $b;
		$this->c = $c;
	}
}

$o = new Obj(1, 2, 3);


test('object', $o);

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
1c034f626a19030e016104010e04002a006204020e06004f626a00630403
OK

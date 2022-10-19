--TEST--
Recursive objects v3
--INI--
error_reporting = E_NONE
igbinary.use_v3_serialize_format=1
--FILE--
<?php
if(!extension_loaded('igbinary')) {
	dl('igbinary.' . PHP_SHLIB_SUFFIX);
}

function test($type, $variable, $test) {
	$serialized = igbinary_serialize($variable);
	$unserialized = igbinary_unserialize($serialized);
//	$serialized = serialize($variable);
//	$unserialized = unserialize($serialized);

	echo $type, "\n";
	echo bin2hex($serialized), "\n";
//	echo $serialized, "\n";
	echo $test || $unserialized == $variable ? 'OK' : 'ERROR';
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

class Obj2 {
	public $aa;
	protected $bb;
	private $cc;
	private $obj;

	function __construct($a, $b, $c) {
		$this->a = $a;
		$this->b = $b;
		$this->c = $c;

		$this->obj = new Obj($a, $b, $c);
	}
}

class Obj3 {
	private $objs;

	function __construct($a, $b, $c) {
		$this->objs = array();

		for ($i = $a; $i < $c; $i += $b) {
			$this->objs[] = new Obj($a, $i, $c);
		}
	}
}

class Obj4 {
	private $a;
	private $obj;

	function __construct($a) {
		$this->a = $a;
	}

	public function set($obj) {
		$this->obj = $obj;
	}
}

$o2 = new Obj2(1, 2, 3);
test('objectrec', $o2, false);

$o3 = new Obj3(0, 1, 4);
test('objectrecarr', $o3, false);

$o4 = new Obj4(100);
$o4->set($o4);
test('objectselfrec', $o4, true);

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
objectrec
000000031c044f626a3219070e026161010e05002a006262010e08004f626a32006363010e09004f626a32006f626a1c034f626a19030e016104010e04002a006204020e06004f626a00630403120604010e016204020e01630403
OK
objectrecarr
000000031c044f626a3319010e0a004f626a33006f626a73190404001c034f626a19030e016104000e04002a006204000e06004f626a00630404040115021903120304001204040112050404040215021903120304001204040212050404040315021903120304001204040312050404
OK
objectselfrec
000000031c044f626a3419020e07004f626a34006104640e09004f626a34006f626a2400
OK

--TEST--
Serialize object into session, full set v3
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php
if (!extension_loaded('session')) {
	exit('skip session extension not loaded');
}

ob_start();
phpinfo(INFO_MODULES);
$str = ob_get_clean();

$array = explode("\n", $str);
$array = preg_grep('/^igbinary session support.*yes/', $array);
if (!$array) {
	exit('skip igbinary session handler not available');
}


--FILE--
<?php

class Foo {
	private static $s1 = array();
	protected static $s2 = array();
	public static $s3 = array();

	private $d1;
	protected $d2;
	public $d3;

	public function __construct($foo) {
		$this->d1 = $foo;
		$this->d2 = $foo;
		$this->d3 = $foo;
	}
}

class Bar {
	private static $s1 = array();
	protected static $s2 = array();
	public static $s3 = array();

	public $d1;
	private $d2;
	protected $d3;

	public function __construct() {
	}

	public function set($foo) {
		$this->d1 = $foo;
		$this->d2 = $foo;
		$this->d3 = $foo;
	}
}

if(!extension_loaded('igbinary')) {
	dl('igbinary.' . PHP_SHLIB_SUFFIX);
}

$output = '';

function open($path, $name) {
	return true;
}

function close() {
	return true;
}

function read($id) {
	global $output;
	$output .= "read\n";
	$a = new Bar();
	$b = new Foo($a);
	$a->set($b);
	$session = array('old' => $b);
	return igbinary_serialize($session);
}

function write($id, $data) {
	global $output;
	$output .= "write: ";
	$output .= bin2hex($data) . "\n";
	return true;
}

function destroy($id) {
	return true;
}

function gc($time) {
	return true;
}

ini_set('session.serialize_handler', 'igbinary');

session_set_save_handler('open', 'close', 'read', 'write', 'destroy', 'gc');

session_start();

$_SESSION['test'] = "foobar";
$a = new Bar();
$b = new Foo($a);
$a->set($b);
$_SESSION['new'] = $a;

session_write_close();

echo $output;

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
read
write: 0000000319030e036f6c641c03466f6f19030e0700466f6f0064311c0342617219030e02643124010e070042617200643224010e05002a00643324010e05002a00643224020e02643324020e04746573740e06666f6f6261720e036e6577150319031204150119031202240312072403120824031205240412062404

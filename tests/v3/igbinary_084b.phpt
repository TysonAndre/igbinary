--TEST--
Properly free duplicate undeclared properties when unserializing invalid data
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php
if (PHP_VERSION_ID >= 90000) { echo "skip requires php < 9.0 when testing that the deprecation has no impact on igbinary functionality\n"; }
?>
--FILE--
<?php
if (PHP_VERSION_ID >= 80200) { require_once __DIR__ . '/../php82_suppress_dynamic_properties_warning.inc'; }
class Test {
    public function __construct( ) {
        $this->pub = null;
    }

    public function __sleep() {
        // TODO: Could start detecting duplicates and emitting a notice as well
        return ["pub", "pub"];
    }
}
$t = new Test();
$t->pub = new Test();
$s = igbinary_serialize($t);
echo urlencode($s), "\n";
$unser = igbinary_unserialize($s);
var_dump($unser);
?>
--EXPECT--
%00%00%00%03%1C%04Test%19%02%0E%03pub%15%00%19%02%12%01%01%12%01%01%12%01%24%01
object(Test)#3 (1) {
  ["pub"]=>
  object(Test)#4 (1) {
    ["pub"]=>
    NULL
  }
}
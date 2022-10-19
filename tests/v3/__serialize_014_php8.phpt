--TEST--
__serialize() mechanism (014): Uninitialized properties can be serialized and unserialized
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php if (PHP_VERSION_ID < 80000) { echo "skip __serialize/__unserialize error message different in php < 8"; } ?>
--FILE--
<?php
class MyClass {
    public stdClass $o;
    public string $s;
    public ?int $i;
}
// 00000003               -- header
// 17 07 4d79436c617373   -- object of type "MyClass"
//   14 03 000000           -- with 3 uninitialized properties
$m = new MyClass();
var_dump($m);
var_dump(bin2hex($s = igbinary_serialize($m)));
var_dump(igbinary_unserialize($s));
$m = new MyClass();
$m->o = new stdClass();
unset($m->o);
$m->s = 'other';
unset($m->s);
$m->i = 42;
unset($m->i);
// Should have the same serialized representation.
var_dump($m);
var_dump(bin2hex($s = igbinary_serialize($m)));
try {
    $m->i = 'i';
} catch (TypeError $e) {
    echo $e->getMessage() . "\n";
}
--EXPECT--
object(MyClass)#1 (0) {
  ["o"]=>
  uninitialized(stdClass)
  ["s"]=>
  uninitialized(string)
  ["i"]=>
  uninitialized(?int)
}
string(36) "000000031c074d79436c6173731903010101"
object(MyClass)#2 (0) {
  ["o"]=>
  uninitialized(stdClass)
  ["s"]=>
  uninitialized(string)
  ["i"]=>
  uninitialized(?int)
}
object(MyClass)#2 (0) {
  ["o"]=>
  uninitialized(stdClass)
  ["s"]=>
  uninitialized(string)
  ["i"]=>
  uninitialized(?int)
}
string(36) "000000031c074d79436c6173731903010101"
Cannot assign string to property MyClass::$i of type ?int
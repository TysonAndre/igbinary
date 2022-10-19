--TEST--
Test serializing wrong values in __sleep
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php if (!extension_loaded("igbinary")) print "skip"; ?>
--FILE--
<?php
#[AllowDynamicProperties]
class X {
    public function __sleep() {
        return $this;
    }
}
$x = new X();
for ($i = 0; $i < 3; $i++) {
    $x->{"p$i"} = "name$i";
}
$ser = igbinary_serialize($x);
$unser = igbinary_unserialize($ser);
echo str_replace(['\\', '%'], ['\\\\', '\x'], urlencode($ser)), "\n";
var_dump($unser);

?>
--EXPECTF--
Notice: igbinary_serialize(): "name0" returned as member variable from __sleep() but does not exist in %s on line 12

Notice: igbinary_serialize(): "name1" returned as member variable from __sleep() but does not exist in %s on line 12

Notice: igbinary_serialize(): "name2" returned as member variable from __sleep() but does not exist in %s on line 12
\x00\x00\x00\x03\x1C\x01X\x19\x03\x0E\x05name0\x01\x0E\x05name1\x01\x0E\x05name2\x01
object(X)#2 (3) {
  ["name0"]=>
  NULL
  ["name1"]=>
  NULL
  ["name2"]=>
  NULL
}
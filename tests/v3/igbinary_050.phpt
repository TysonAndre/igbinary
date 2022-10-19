--TEST--
Correctly unserialize cyclic object references
--INI--
igbinary.use_v3_serialize_format=1
igbinary.compact_strings = On
--FILE--
<?php
$a = new stdClass();
$a->foo = &$a;
$a->bar = &$a;
$b = new stdClass();
$b->cyclic = &$a;
printf("%s\n", serialize($b));
$ig_ser = igbinary_serialize($b);
printf("%s\n", bin2hex($ig_ser));
$ig = igbinary_unserialize($ig_ser);
printf("%s\n", serialize($ig));
var_dump($ig);
$f = &$ig->cyclic->foo;
$f = 'V';
var_dump($ig);
// Note: While the php7 unserializer consistently makes a distinction between refs to an object and non-refs,
// the php5 serializer does not.
--EXPECTF--
O:8:"stdClass":1:{s:6:"cyclic";O:8:"stdClass":2:{s:3:"foo";R:2;s:3:"bar";R:2;}}
000000031c08737464436c61737319010e066379636c696322150019020e03666f6f2224010e03626172222401
O:8:"stdClass":1:{s:6:"cyclic";O:8:"stdClass":2:{s:3:"foo";R:2;s:3:"bar";R:2;}}
object(stdClass)#3 (1) {
  ["cyclic"]=>
  &object(stdClass)#4 (2) {
    ["foo"]=>
    *RECURSION*
    ["bar"]=>
    *RECURSION*
  }
}
object(stdClass)#3 (1) {
  ["cyclic"]=>
  &string(1) "V"
}

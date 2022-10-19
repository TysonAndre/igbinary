--TEST--
igbinary and __PHP_INCOMPLETE_CLASS
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
// TODO: Remove temporary workaround for __PHP_Incomplete_Class missing #[AllowDynamicProperties]
if (PHP_VERSION_ID >= 80200) { require_once __DIR__ . '/../php82_suppress_dynamic_properties_warning.inc'; }
class Test {}
function test_ser_unser($obj) {
    var_dump(bin2hex($s = igbinary_serialize($obj)));
    $s = str_replace('Test', 'Best', $s);
    $obj2 = igbinary_unserialize($s);
    var_dump($obj2);
    var_dump(bin2hex($s = igbinary_serialize($obj2)));
    var_dump(igbinary_unserialize($s));
}
test_ser_unser(new Test());
echo "Testing with properties\n";
$obj = new Test();
$obj->dynamicProp = 'value';
$obj->nullProp = null;
test_ser_unser($obj);
?>
--EXPECT--
string(24) "000000031c04546573741900"
object(__PHP_Incomplete_Class)#2 (1) {
  ["__PHP_Incomplete_Class_Name"]=>
  string(4) "Best"
}
string(24) "000000031c04426573741900"
object(__PHP_Incomplete_Class)#3 (1) {
  ["__PHP_Incomplete_Class_Name"]=>
  string(4) "Best"
}
Testing with properties
string(86) "000000031c045465737419020e0b64796e616d696350726f700e0576616c75650e086e756c6c50726f7001"
object(__PHP_Incomplete_Class)#1 (3) {
  ["__PHP_Incomplete_Class_Name"]=>
  string(4) "Best"
  ["dynamicProp"]=>
  string(5) "value"
  ["nullProp"]=>
  NULL
}
string(86) "000000031c044265737419020e0b64796e616d696350726f700e0576616c75650e086e756c6c50726f7001"
object(__PHP_Incomplete_Class)#3 (3) {
  ["__PHP_Incomplete_Class_Name"]=>
  string(4) "Best"
  ["dynamicProp"]=>
  string(5) "value"
  ["nullProp"]=>
  NULL
}
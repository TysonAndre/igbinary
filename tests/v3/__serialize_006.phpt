--TEST--
__serialize() mechanism (006): DateTime
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php if (PHP_VERSION_ID < 70400) { echo "skip __serialize/__unserialize not supported in php < 7.4 for compatibility with serialize()"; } ?>
--FILE--
<?php

$dt = new DateTime('2019-12-08 12:34', new DateTimeZone('UTC'));
var_dump(bin2hex($s = igbinary_serialize($dt)));
var_dump(igbinary_unserialize($s));
$dt = new DateTime('2019-12-08 12:34', new DateTimeZone('Pacific/Nauru'));
var_dump(bin2hex($s = igbinary_serialize($dt)));
var_dump(igbinary_unserialize($s));

?>
--EXPECT--
string(164) "000000031c084461746554696d6519030e04646174650e1a323031392d31322d30382031323a33343a30302e3030303030300e0d74696d657a6f6e655f7479706504030e0874696d657a6f6e650e03555443"
object(DateTime)#2 (3) {
  ["date"]=>
  string(26) "2019-12-08 12:34:00.000000"
  ["timezone_type"]=>
  int(3)
  ["timezone"]=>
  string(3) "UTC"
}
string(184) "000000031c084461746554696d6519030e04646174650e1a323031392d31322d30382031323a33343a30302e3030303030300e0d74696d657a6f6e655f7479706504030e0874696d657a6f6e650e0d506163696669632f4e61757275"
object(DateTime)#1 (3) {
  ["date"]=>
  string(26) "2019-12-08 12:34:00.000000"
  ["timezone_type"]=>
  int(3)
  ["timezone"]=>
  string(13) "Pacific/Nauru"
}
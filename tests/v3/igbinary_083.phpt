--TEST--
igbinary object with reference to ArrayObject
--INI--
; Note that php 8.1 deprecates using Serializable without __serialize/__unserialize but we are testing Serialize for igbinary. Suppress deprecations.
error_reporting=E_ALL & ~E_DEPRECATED
igbinary.use_v3_serialize_format=1
--FILE--
<?php

class UnSerializable implements Serializable
{
    public function serialize() {
        echo "Called serialize\n";
    }
    public function unserialize($serialized) {
        echo "Called unserialize\n";
    }
}

$unser = new UnSerializable();
$arr = [$unser];
$arr[1] = &$arr[0];
$arr[2] = 'endcap';
$arr[3] = &$arr[2];

$data = igbinary_serialize($arr);
echo urlencode($data) . PHP_EOL;
$recovered = igbinary_unserialize($data);
var_dump($recovered);
?>
--EXPECT--
Called serialize
%00%00%00%03%19%04%04%00%22%01%04%01%22%24%01%04%02%22%0E%06endcap%04%03%22%24%02
array(4) {
  [0]=>
  &NULL
  [1]=>
  &NULL
  [2]=>
  &string(6) "endcap"
  [3]=>
  &string(6) "endcap"
}
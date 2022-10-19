--TEST--
igbinary and many arrays
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
function generate_test_array(int $n) : array {
    $result = [];
    for ($i = 0; $i < $n; $i++) {
        $result[] = [$i];
    }
    // Validate that igbinary properly serializes and unserializes the references to arrays created earlier
    for ($i = 0; $i < $n; $i++) {
        $result[] = $result[$i];
    }
    return $result;
}
$small = generate_test_array(2);
var_dump(bin2hex($s = igbinary_serialize($small)));
var_dump(igbinary_unserialize($s));
$medium = generate_test_array(1 << 8);
var_dump(igbinary_unserialize(igbinary_serialize($medium)) === $medium);
$large = generate_test_array(1 << 16);
var_dump(igbinary_unserialize(igbinary_serialize($large)) === $large);
?>
--EXPECT--
string(60) "000000031904040019010400040004011901040004010402240104032402"
array(4) {
  [0]=>
  array(1) {
    [0]=>
    int(0)
  }
  [1]=>
  array(1) {
    [0]=>
    int(1)
  }
  [2]=>
  array(1) {
    [0]=>
    int(0)
  }
  [3]=>
  array(1) {
    [0]=>
    int(1)
  }
}
bool(true)
bool(true)
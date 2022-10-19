--TEST--
__serialize() mechanism in igbinary
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php
if (PHP_VERSION_ID < 70400) {
    die('skip requires php 7.4+');
}
?>
--FILE--
<?php
class Test {
    public $prop;
    public $prop2;
    public function __serialize() {
        return ["value" => $this->prop, 42 => $this->prop2];
    }
    public function __unserialize(array $data) {
        $this->prop = 'unser' . $data["value"];
        $this->prop2 = 'unser' . $data[42];
    }
}
$test = new Test;
$test->prop = "foobar";
$test->prop2 = "barfoo";
$s = igbinary_serialize($test);
echo bin2hex($s) . "\n";
var_dump(igbinary_unserialize($s));
?>
--EXPECT--
000000031c045465737419020e0576616c75650e06666f6f626172042a0e06626172666f6f
object(Test)#2 (2) {
  ["prop"]=>
  string(11) "unserfoobar"
  ["prop2"]=>
  string(11) "unserbarfoo"
}
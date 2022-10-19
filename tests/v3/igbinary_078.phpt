--TEST--
igbinary and large arrays
--INI--
igbinary.use_v3_serialize_format=1
--FILE--
<?php
class BadSleep {
    public $prop = 'x';
    public function __construct($value) {
        $this->prop = $value;
    }
    public function __sleep() {
        return null;
    }
}
var_dump(bin2hex($s = igbinary_serialize(new BadSleep('override'))));
var_dump(igbinary_unserialize($s));
?>
--EXPECTF--
Notice: igbinary_serialize(): __sleep should return an array only containing the names of instance-variables to serialize in %s on line 11
string(32) "000000031c08426164536c6565701900"
object(BadSleep)#1 (1) {
  ["prop"]=>
  string(1) "x"
}
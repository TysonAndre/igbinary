--TEST--
__serialize() mechanism (007): handle __unserialize throwing
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php if (PHP_VERSION_ID < 70400) { echo "skip __serialize/__unserialize not supported in php < 7.4 for compatibility with serialize()"; } ?>
--FILE--
<?php

class Test {
    public $prop;
    public $prop2;
    public function __serialize() {
        return [$this->prop, $this->prop2];
    }
    public function __unserialize(array $data) {
        $this->prop = $data[0];
        $this->prop2 = $data[1];
        throw new RuntimeException($this->prop);
    }

    public function __destruct() {
        // should not be called
        echo "Called destruct prop=$this->prop\n";
    }
}

$test = new Test;
$test->prop = 'XX';
$test->prop2 = [$test];

// 00000003          - igbinary header v3
// 1c 04 54657374    - object of class with name "Test"
//   19 02           - 2 properties
//     04 00         - uint8(0) =>
//       0e 02 5858  -   'XX'
//     04 01         - uint8(1) =>
//       19 01       -   array(size=1)
//         04 00     - uint8(0) =>
//         24 00     - igbinary_type_objref8 (pointer to the first referenceable item, i.e. the instance of "Test"
var_dump(bin2hex($s = igbinary_serialize($test)));
try {
    var_dump(igbinary_unserialize($s));
} catch (RuntimeException $e) {
    echo "Caught: {$e->getMessage()}\n";
}
$test->prop = 'not from igbinary_unserialize';

?>
--EXPECT--
string(52) "000000031c0454657374190204000e0258580401190104002400"
Caught: XX
Called destruct prop=not from igbinary_unserialize

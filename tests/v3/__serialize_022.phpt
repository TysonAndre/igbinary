--TEST--
__serialize() mechanism (021): Test __serialize without __unserialize
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php if (PHP_VERSION_ID < 70400) { echo "skip __serialize/__unserialize not supported in php < 7.4 for compatibility with serialize()"; } ?>
--FILE--
<?php
#[AllowDynamicProperties]
class Test {
    const SOME_CONST = ['key' => 'value'];

    public function __serialize() {
        return self::SOME_CONST;
    }

    public static function test_serialize() {
        $x = igbinary_serialize([self::SOME_CONST, new self(), new self()]);
        // aggressively reuses arrays
        echo urlencode($x), "\n";
        var_dump(igbinary_unserialize($x));
    }
}

Test::test_serialize();
?>
--EXPECT--
%00%00%00%03%19%03%04%00%19%01%0E%03key%0E%05value%04%01%1C%04Test%19%01%12%00%12%01%04%02%15%02%19%01%12%00%12%01
array(3) {
  [0]=>
  array(1) {
    ["key"]=>
    string(5) "value"
  }
  [1]=>
  object(Test)#2 (1) {
    ["key"]=>
    string(5) "value"
  }
  [2]=>
  object(Test)#1 (1) {
    ["key"]=>
    string(5) "value"
  }
}
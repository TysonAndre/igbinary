--TEST--
Test handling php 8.1 readonly properties
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php if (PHP_VERSION_ID < 80100) { echo "skip readonly properties require php 8.1+\n"; } ?>
--FILE--
<?php
class X {
    public readonly mixed $var;

    public function __construct(
        public readonly int $a,
        private readonly ArrayAccess&Countable $intersection,
        protected readonly ?string $default = null,
    ) {
        $this->var = $intersection;
    }
}

class Y {
    public readonly mixed $var;

    public function __construct(
        public readonly int $a,
        private readonly ArrayAccess&Countable $intersection,
        protected readonly ?string $default = null,
    ) {
        $this->var = $intersection;
    }

    public function __serialize(): array {
        return [
            'a' => $this->a,
            'intersection' => $this->intersection,
            'default' => $this->default,
            'var' => $this->var,
        ];
    }
    public function __unserialize(array $data) {
        [
            'a' => $this->a,
            'intersection' => $this->intersection,
            'default' => $this->default,
            'var' => $this->var,
        ] = $data;
    }
}

$ser = igbinary_serialize(new X(1, new ArrayObject()));
echo urlencode($ser), "\n";
var_dump(igbinary_unserialize($ser));
$ser = igbinary_serialize(new Y(1, new ArrayObject()));
echo urlencode($ser), "\n";
var_dump(igbinary_unserialize($ser));
?>
--EXPECT--
%00%00%00%03%1C%01X%19%04%0E%03var%1C%0BArrayObject%19%04%04%00%04%00%04%01%19%00%04%02%19%00%04%03%01%0E%01a%04%01%0E%0F%00X%00intersection%24%01%0E%0A%00%2A%00default%01
object(X)#1 (4) {
  ["var"]=>
  object(ArrayObject)#2 (1) {
    ["storage":"ArrayObject":private]=>
    array(0) {
    }
  }
  ["a"]=>
  int(1)
  ["intersection":"X":private]=>
  object(ArrayObject)#2 (1) {
    ["storage":"ArrayObject":private]=>
    array(0) {
    }
  }
  ["default":protected]=>
  NULL
}
%00%00%00%03%1C%01Y%19%04%0E%01a%04%01%0E%0Cintersection%1C%0BArrayObject%19%04%04%00%04%00%04%01%19%00%04%02%19%00%04%03%01%0E%07default%01%0E%03var%24%01
object(Y)#1 (4) {
  ["var"]=>
  object(ArrayObject)#2 (1) {
    ["storage":"ArrayObject":private]=>
    array(0) {
    }
  }
  ["a"]=>
  int(1)
  ["intersection":"Y":private]=>
  object(ArrayObject)#2 (1) {
    ["storage":"ArrayObject":private]=>
    array(0) {
    }
  }
  ["default":protected]=>
  NULL
}
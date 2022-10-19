--TEST--
Works when there are hash collisions in strings when serializing.
--INI--
igbinary.use_v3_serialize_format=1
--SKIPIF--
<?php
if(!extension_loaded('igbinary')) {
    echo "skip no igbinary";
}
?>
--FILE--
<?php

#[AllowDynamicProperties]
class Fy{
    public $EzFy = 2;
    public function __construct($x) {
        $this->x = $x;
    }
}
class Ez {
    public $FyEz = 'EzEz';
}

class G8 {
    public $FyG8;
}

$data = array(new Fy('G8G8'), new Fy('EzG8'), new Ez(), new G8(), new Ez(), 'G8' => new G8(), 'F8Ez' => new G8(), array(new G8()));
var_dump($data);
echo "\n";
$str = igbinary_serialize($data);
echo bin2hex($str) . "\n";
$unserialized = igbinary_unserialize($str);
var_dump($unserialized);
echo "\n";
var_export(serialize($data) === serialize($unserialized));
?>
--EXPECT--
array(8) {
  [0]=>
  object(Fy)#1 (2) {
    ["EzFy"]=>
    int(2)
    ["x"]=>
    string(4) "G8G8"
  }
  [1]=>
  object(Fy)#2 (2) {
    ["EzFy"]=>
    int(2)
    ["x"]=>
    string(4) "EzG8"
  }
  [2]=>
  object(Ez)#3 (1) {
    ["FyEz"]=>
    string(4) "EzEz"
  }
  [3]=>
  object(G8)#4 (1) {
    ["FyG8"]=>
    NULL
  }
  [4]=>
  object(Ez)#5 (1) {
    ["FyEz"]=>
    string(4) "EzEz"
  }
  ["G8"]=>
  object(G8)#6 (1) {
    ["FyG8"]=>
    NULL
  }
  ["F8Ez"]=>
  object(G8)#7 (1) {
    ["FyG8"]=>
    NULL
  }
  [5]=>
  array(1) {
    [0]=>
    object(G8)#8 (1) {
      ["FyG8"]=>
      NULL
    }
  }
}

00000003190804001c02467919020e04457a467904020e01780e04473847380401150019021201040212020e04457a473804021c02457a19010e044679457a0e04457a457a04031c02473819010e044679473801040415051901120612071208150819011209010e044638457a1508190112090104051901040015081901120901
array(8) {
  [0]=>
  object(Fy)#9 (2) {
    ["EzFy"]=>
    int(2)
    ["x"]=>
    string(4) "G8G8"
  }
  [1]=>
  object(Fy)#10 (2) {
    ["EzFy"]=>
    int(2)
    ["x"]=>
    string(4) "EzG8"
  }
  [2]=>
  object(Ez)#11 (1) {
    ["FyEz"]=>
    string(4) "EzEz"
  }
  [3]=>
  object(G8)#12 (1) {
    ["FyG8"]=>
    NULL
  }
  [4]=>
  object(Ez)#13 (1) {
    ["FyEz"]=>
    string(4) "EzEz"
  }
  ["G8"]=>
  object(G8)#14 (1) {
    ["FyG8"]=>
    NULL
  }
  ["F8Ez"]=>
  object(G8)#15 (1) {
    ["FyG8"]=>
    NULL
  }
  [5]=>
  array(1) {
    [0]=>
    object(G8)#16 (1) {
      ["FyG8"]=>
      NULL
    }
  }
}

true

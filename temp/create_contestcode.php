<?php


// usage: php create_contestcode.php PE1ITR 2222 432

$pcall = $argv[1];
$code = $argv[2];
$pband = $argv[3];
$model = $argv[4];  // standaard model 1

echo "creating image \n";
if (isset($argv[1])) { echo "callsign=" . $argv[1] ."\n"; } else { die('no callsign. usage php create_contestcode.php PE1ITR 2222 432 1'); }
if (isset($argv[2])) { echo "code=" . $argv[2] . "\n"; } else { die('no code'); }
if (isset($argv[3])) { echo "band=" . $argv[3] . "\n"; } else { die('no band'); }
if (isset($argv[4])) { echo "model=" . $argv[4] . "\n"; } else { die('no model. 1 is met kleurenbalk'); }

$prefix = explode("/",$argv[1]);


switch ($prefix[0]) {
	case "DL":
		$maidenheadlocator = "JO30DQ50PT";
		break;
	case "LX":
		$maidenheadlocator = "JO30BA24PW";
		break;
	case "F":
		$maidenheadlocator = "JN39FL14OH";
		break;
	case "HB9":
		$maidenheadlocator = "JN47CF";
		break;
	default:
		$maidenheadlocator = "JO21QK86DV";
		}
		
$code = substr($code,0,4);

$code1 = rand(1,9);
$code2 = rand(0,9);
$code3 = rand(0,9);
$code4 = rand(0,9);


while ( $code1 == $code2 or $code1 == ($code2 - 1) or ($code1 - 1) == $code2 ) {
$code2 = rand(0,9);
}
while ( $code2 == $code3 or $code2 == ($code3 - 1) or ($code2 - 1) == $code3 or $code1 == $code3 ) {
$code3 = rand(0,9);
}
while ( $code3 == $code4 or $code3 == ($code4 - 1) or ($code3 - 1) == $code4 or $code1 == $code4 or $code2 == $code4 ) {
$code4 = rand(0,9);
}

$code = ($code1 * 1000) + ($code2 * 100) + ($code3 * 10) + $code4; 
$desom = $code1 + $code2 + $code3 + $code4;

echo "code=" . $code . "\n";
echo "som=" . $desom . "\n";



$font = '/usr/share/fonts/gnu-free/FreeSansBold.ttf';
//$font = '/usr/share/fonts/truetype/freefont/FreeSansBold.ttf';
$fontsize = 120;
$fontsize_code = 360;
$fontsize_code2 = 600;
$fontsize_band = 32;
$xpixs = 1080;
$ypixs =  810;



$t = 0;

/*
t = 0 normaal
t = 1 inverse
t = 2 eerste twee cijfers
t = 3 laatste twee cijfers
*/

while ( $t <= 0 ) {
$im = imagecreate($xpixs,$ypixs);


// achtergrond
if ( $t == 1 ) { 
$background_color = imagecolorallocate($im,255,255,255);
$text_color = imagecolorallocate($im,0,0,0);
} else {
$background_color = imagecolorallocate($im,0,0,0);
$text_color = imagecolorallocate($im,255,255,255);
}

// kleurenblak
$gray=imagecolorallocate($im,104,104,104);
$yellow=imagecolorallocate($im,180,180,16);
$cyan=imagecolorallocate($im,16,180,180);
$green=imagecolorallocate($im,16,180,16);
$magenta=imagecolorallocate($im,180,16,180);
$red=imagecolorallocate($im,180,16,16);
$blue=imagecolorallocate($im,16,16,180);
$white=imagecolorallocate($im,235,235,235);
$black=imagecolorallocate($im,16,16,16);

if ($model == 1 ) {
    $hoogte=40;
    $breedte=$xpixs/8;
    imagefilledrectangle ($im,0,0,$breedte*1,$hoogte,$white);
    imagefilledrectangle ($im,$breedte*1,0,$breedte*2,$hoogte,$yellow);
    imagefilledrectangle ($im,$breedte*2,0,$breedte*3,$hoogte,$cyan);
    imagefilledrectangle ($im,$breedte*3,0,$breedte*4,$hoogte,$green);
    imagefilledrectangle ($im,$breedte*4,0,$breedte*5,$hoogte,$magenta);
    imagefilledrectangle ($im,$breedte*5,0,$breedte*6,$hoogte,$red);
    imagefilledrectangle ($im,$breedte*6,0,$breedte*7,$hoogte,$blue);
    imagefilledrectangle ($im,$breedte*7,0,$breedte*8,$hoogte,$black);

    //frequentiebalk
    $hoogte=$ypixs-40;
    $breedte=$xpixs/8;

    imagefilledrectangle ($im,0,$hoogte,$breedte,$ypixs,$white);
    imagefilledrectangle ($im,$breedte*1,$hoogte,$breedte*2,$ypixs,$black);

    $breedte2=$xpixs/16;
    imagefilledrectangle ($im,$breedte*2,$hoogte,$breedte*2+($breedte2*1),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*2+($breedte2*1),$hoogte,$breedte*2+($breedte2*2),$ypixs,$black);
    imagefilledrectangle ($im,$breedte*2+($breedte2*2),$hoogte,$breedte*2+($breedte2*3),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*2+($breedte2*3),$hoogte,$breedte*2+($breedte2*4),$ypixs,$black);

    $breedte2=$xpixs/32;
    imagefilledrectangle ($im,$breedte*4,$hoogte,$breedte*4+($breedte2*1),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*4+($breedte2*1),$hoogte,$breedte*4+($breedte2*2),$ypixs,$black);
    imagefilledrectangle ($im,$breedte*4+($breedte2*2),$hoogte,$breedte*4+($breedte2*3),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*4+($breedte2*3),$hoogte,$breedte*4+($breedte2*4),$ypixs,$black);
    imagefilledrectangle ($im,$breedte*4+($breedte2*4),$hoogte,$breedte*4+($breedte2*5),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*4+($breedte2*5),$hoogte,$breedte*4+($breedte2*6),$ypixs,$black);
    imagefilledrectangle ($im,$breedte*4+($breedte2*6),$hoogte,$breedte*4+($breedte2*7),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*4+($breedte2*7),$hoogte,$breedte*4+($breedte2*8),$ypixs,$black);

    $breedte2=$xpixs/64;
    imagefilledrectangle ($im,$breedte*6,$hoogte,$breedte*6+($breedte2*1),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*6+($breedte2*1),$hoogte,$breedte*6+($breedte2*2),$ypixs,$black);
    imagefilledrectangle ($im,$breedte*6+($breedte2*2),$hoogte,$breedte*6+($breedte2*3),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*6+($breedte2*3),$hoogte,$breedte*6+($breedte2*4),$ypixs,$black);
    imagefilledrectangle ($im,$breedte*6+($breedte2*4),$hoogte,$breedte*6+($breedte2*5),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*6+($breedte2*5),$hoogte,$breedte*6+($breedte2*6),$ypixs,$black);
    imagefilledrectangle ($im,$breedte*6+($breedte2*6),$hoogte,$breedte*6+($breedte2*7),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*6+($breedte2*7),$hoogte,$breedte*6+($breedte2*8),$ypixs,$black);
    imagefilledrectangle ($im,$breedte*6+($breedte2*8),$hoogte,$breedte*6+($breedte2*9),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*6+($breedte2*9),$hoogte,$breedte*6+($breedte2*10),$ypixs,$black);
    imagefilledrectangle ($im,$breedte*6+($breedte2*10),$hoogte,$breedte*6+($breedte2*11),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*6+($breedte2*11),$hoogte,$breedte*6+($breedte2*12),$ypixs,$black);
    imagefilledrectangle ($im,$breedte*6+($breedte2*12),$hoogte,$breedte*6+($breedte2*13),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*6+($breedte2*13),$hoogte,$breedte*6+($breedte2*14),$ypixs,$black);
    imagefilledrectangle ($im,$breedte*6+($breedte2*14),$hoogte,$breedte*6+($breedte2*15),$ypixs,$white);
    imagefilledrectangle ($im,$breedte*6+($breedte2*15),$hoogte,$breedte*6+($breedte2*16),$ypixs,$black);
    }


$aantalchar = strlen($pcall);
$startpos = 300;
if ( $aantalchar == 5 ) { $startpos = 300; }
if ( $aantalchar == 6 ) { $startpos = 250; }
if ( $aantalchar == 7 ) { $startpos = 200; }
if ( $aantalchar == 8 ) { $startpos = 150; }
if ( $aantalchar == 9 ) { $startpos = 100; }

$band = $pband;
if ( $pband == 50 ) { $band = "51.7 MHz"; }
if ( $pband == 70 ) { $band = "4m"; }
if ( $pband == 144 ) { $band = "144.6 MHz"; }
if ( $pband == 432 ) { $band = "70cm"; }
if ( $pband == 1296 ) { $band = "23cm"; }
if ( $pband == 2320 ) { $band = "13cm"; }
if ( $pband == 3400 ) { $band = "9cm"; }
if ( $pband == 5760 ) { $band = "6cm"; }
if ( $pband == 10368 ) { $band = "3cm"; }

$pcallfn = str_replace("/","_SLASH_",$pcall);
$band = $band . " - " . $maidenheadlocator . " - som = " . $desom . " - TNX QSO! 73";


switch ($t) {
    case 0:
	imagettftext($im,$fontsize,0,$startpos,190,$text_color,$font,$pcall);
	imagettftext($im,$fontsize_code,0,10,630,$text_color,$font,$code);
	imagettftext($im,$fontsize_band,0,10,$ypixs-80,$text_color,$font,$band);
        imagejpeg($im,'data/' . $pcallfn . '-contestcode-' . $pband . '-' . $code . '-norm.jpg');
        break;
    case 1:
	imagettftext($im,$fontsize,0,$startpos,190,$text_color,$font,$pcall);
	imagettftext($im,$fontsize_code,0,10,630,$text_color,$font,$code);
	imagettftext($im,$fontsize_band,0,10,$ypixs-10,$text_color,$font,$band);
        imagejpeg($im,'data/' . $pcallfn . '-contestcode-' . $pband . '-' . $code . '-invers.jpg');
        break;
    case 2:
	imagettftext($im,$fontsize_code2,0,50,680,$text_color,$font,substr($code,0,2));
	imagettftext($im,$fontsize_band,0,10,$ypixs-10,$text_color,$font,$band);
        imagejpeg($im,'data/' . $pcallfn . '-contestcode-' . $pband . '-' . $code . '-eerste2.jpg');
        break;
    case 3:
	imagettftext($im,$fontsize_code2,0,50,680,$text_color,$font,substr($code,2,2));
	imagettftext($im,$fontsize_band,0,10,$ypixs-10,$text_color,$font,$band);
        imagejpeg($im,'data/' . $pcallfn . '-contestcode-' . $pband .  '-' . $code . '-laatste2.jpg');
        break;
    default:
	imagettftext($im,$fontsize,0,$startpos,190,$text_color,$font,$pcall);
	imagettftext($im,$fontsize_code,0,10,630,$text_color,$font,$code);
	imagettftext($im,$fontsize_band,0,10,$ypixs-10,$text_color,$font,$band);
	imagejpeg($im,'data/' . $pcallfn . '-contestcode-' . $pband . '-' . $code .  '-norm.jpg');
}


imagedestroy($im);

$t = $t + 1;

}


echo "ready \n";

?>

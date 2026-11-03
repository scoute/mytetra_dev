<?php

// Создание тестовой базы MyTetra для нагрузочного тестирования.
//
// Использование:
//   php generateBase.php [outDir [levels [branches [notes]]]]
//
//   outDir   - каталог базы (будет УДАЛЕН и создан заново, только он).
//              По умолчанию ./mytetraHighLoad/data (от текущего каталога)
//   levels   - глубина дерева (по умолчанию 3)
//   branches - веток на уровне (по умолчанию 19)
//   notes    - записей в ветке (по умолчанию 15)
//
// Дефолт 19^3*15 = 102885 записей (~100k).
// Живая база не затрагивается: скрипт пишет только в outDir.
// Словарь берется из каталога скрипта, запуск из любого места.
//
// Требуется только базовый PHP (без php-xml и mbstring):
// XML пишется потоково строкой, длина UTF-8 считается через PCRE.

$baseGenerator=new BaseGenerator(
    isset($argv[1]) ? $argv[1] : './mytetraHighLoad/data',
    isset($argv[2]) ? (int) $argv[2] : 3,
    isset($argv[3]) ? (int) $argv[3] : 19,
    isset($argv[4]) ? (int) $argv[4] : 15
);

class BaseGenerator
{
    protected $outDirName;      // Директория, куда будут сложены сгенерированные данные
    protected $dictFileName;    // Словарь используемых слов в текстах
    protected $treeLevels;      // Уровень вложения генерируемого дерева
    protected $branchesQuantity;// Количество веток на одном уровне вложения
    protected $notesQuantity;   // Количество записей в одной ветке
    protected $minSymbolsInTitle=25;  // Примерное минимальное кол-во символов в названии записи
    protected $maxSymbolsInTitle=100; // Примерное максимальное кол-во символов в названии записи
    protected $minSymbolsInTags=10;   // Примерное минимальное кол-во символов в текстовых метках
    protected $maxSymbolsInTags=100;  // Примерное максимальное кол-во символов в текстовых метках
    protected $minSymbolsInNote=2000; // Примерное минимальное кол-во символов в тексте записи
    protected $maxSymbolsInNote=4000; // Примерное максимальное кол-во символов в тексте записи
    protected $minSymbolsInItem=5;    // Примерное минимальное кол-во символов в названии ветки
    protected $maxSymbolsInItem=20;   // Примерное максимальное кол-во символов в названии ветки


    protected $dictWords=[];
    protected $branchCounter=0;
    protected $noteCounter=0;
    protected $xmlHandle=null;


    function __construct($outDirName='./mytetraHighLoad/data', $treeLevels=3, $branchesQuantity=19, $notesQuantity=15)
    {
        $this->outDirName=$outDirName;
        $this->treeLevels=max(1, $treeLevels);
        $this->branchesQuantity=max(1, $branchesQuantity);
        $this->notesQuantity=max(0, $notesQuantity);
        $this->dictFileName=__DIR__.'/generateDictionary.txt';

        $this->printTotalNumberOfNotes();

        $this->createBaseDir();

        $this->loadDict();

        $this->generate();

    }


    // Создание директории с базой
    function createBaseDir()
    {
        if($this->outDirName==='' || $this->outDirName==='/')
        {
            fwrite(STDERR, "Refusing to wipe empty or root directory\n");
            exit(1);
        }

        delDir($this->outDirName); // Удаление предыдущей директории

        if(!mkdir($this->outDirName, 0777, true)) // Создание основной директории данных
        {
            fwrite(STDERR, "Can not create output directory ".$this->outDirName."\n");
            exit(1);
        }

        mkdir($this->outDirName."/base", 0777); // Создание директории для хранения текстов записей
    }


    function loadDict()
    {
        if(!is_file($this->dictFileName))
        {
            fwrite(STDERR, "Dictionary not found: ".$this->dictFileName."\n");
            exit(1);
        }

        $lines=file($this->dictFileName);

        foreach($lines as $line)
        {
            $word=trim($line);

            if($word!=='')
                $this->dictWords[]=$word;
        }

        if(count($this->dictWords)===0)
        {
            fwrite(STDERR, "Dictionary is empty: ".$this->dictFileName."\n");
            exit(1);
        }
    }


    // Предварительный подсчет количества генерируемых записей
    function printTotalNumberOfNotes()
    {
        echo "Total number of notes: ".$this->calculateTotalNumberOfNotes()."\n";
    }


    function calculateTotalNumberOfNotes()
    {
        return ($this->branchesQuantity ** $this->treeLevels) * $this->notesQuantity;
    }


    function generate()
    {
        // XML пишется потоково: на 100k записей держать весь DOM
        // в памяти незачем, QDom-парсеру пробелы не важны
        $xmlFileName=$this->outDirName."/mytetra.xml";
        $this->xmlHandle=fopen($xmlFileName, 'w');

        if($this->xmlHandle===false)
        {
            fwrite(STDERR, "Can not write ".$xmlFileName."\n");
            exit(1);
        }

        fwrite($this->xmlHandle, '<?xml version="1.0" encoding="UTF-8"?>'."\n");
        fwrite($this->xmlHandle, '<!DOCTYPE mytetradoc>'."\n");
        fwrite($this->xmlHandle, '<root><format version="1" subversion="2"/><content>');

        $this->generateTree(1);

        fwrite($this->xmlHandle, '</content></root>'."\n");
        fclose($this->xmlHandle);
        $this->xmlHandle=null;
    }


    function generateTree($level)
    {
        for($i=0; $i<$this->branchesQuantity; $i++) {
            $branchName=$this->generateItemTitle( 'node'.$this->branchCounter++ );
            $branchId=getId();

            fwrite($this->xmlHandle, '<node name="'.$this->xmlEscape($branchName).'" id="'.$this->xmlEscape($branchId).'">');

            // Добавление к ветке элементов записей
            $this->generateNotes();

            if($level < $this->treeLevels) {
                $this->generateTree($level+1);
            }

            fwrite($this->xmlHandle, '</node>');
        }
    }


    function generateNotes()
    {
        fwrite($this->xmlHandle, '<recordtable>');

        for($i=0; $i<$this->notesQuantity; $i++) {
            $noteName=$this->generateNoteTitle( 'record'.$this->noteCounter++ );
            $noteTags=$this->generateNoteTags();
            $noteId=getId();
            $noteDir=getId();

            fwrite($this->xmlHandle, '<record name="'.$this->xmlEscape($noteName).'" tags="'.$this->xmlEscape($noteTags).'" id="'.$this->xmlEscape($noteId).'" dir="'.$this->xmlEscape($noteDir).'" file="text.html"/>');

            //Создание данных на диске для текущей записи (директория, файл с текстом записи)
            $this->createNoteData($noteDir, 'text.html');
        }

        fwrite($this->xmlHandle, '</recordtable>');

        echo 'Generate note '.$this->noteCounter.' from ~'.$this->calculateTotalNumberOfNotes()."\n";
    }


    function createNoteData($dirName, $fileName)
    {
        $resultDirName=$this->outDirName."/base/".$dirName;

        if(!is_dir($resultDirName))
            mkdir($resultDirName, 0777);

        $resultFileName=$resultDirName."/".$fileName;
        $file=fopen($resultFileName, 'w');
        fwrite($file, $this->generateNoteText());
        fclose($file);
    }


    function generateItemTitle($prefixText)
    {
        return $prefixText.' '.$this->generateSmartText($this->minSymbolsInItem, $this->maxSymbolsInItem, ' ');
    }


    function generateNoteTitle($prefixText)
    {
        return $prefixText.' '.$this->generateSmartText($this->minSymbolsInTitle, $this->maxSymbolsInTitle, ' ');
    }


    function generateNoteTags()
    {
        return $this->generateSmartText($this->minSymbolsInTags, $this->maxSymbolsInTags, ', ');
    }


    function generateNoteText()
    {
        $text=$this->generateSmartText($this->minSymbolsInNote, $this->maxSymbolsInNote, ' ');

        $header=<<<'EOTHEADER'
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0//EN" "http://www.w3.org/TR/REC-html40/strict.dtd">
<html><head>
<meta name="qrichtext" content="1" />
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<style type="text/css">p, li { white-space: pre-wrap; }</style>
</head><body style=" font-family:'DejaVu Sans'; font-size:11pt; font-weight:400; font-style:normal;">
<p style=" margin-top:0px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;">
EOTHEADER;

        $footer=<<<'EOTFOOTER'
</p></body></html>
EOTFOOTER;

        return $header.$this->xmlEscape($text).$footer;
    }


    function generateSmartText($minLen, $maxLen, $delimeter)
    {
        $targetTextLen=rand($minLen, $maxLen);
        $text='';

        while($this->utf8Length($text)<$targetTextLen) {
            $randomWord=$this->dictWords[ rand(0, ( count($this->dictWords))-1 ) ];

            if($text==='') {
                $text.=$randomWord;
            }
            else {
                $text.=$delimeter.$randomWord;
            }
        }

        return trim($text);
    }


    // Экранирование для XML: без php-xml, только ядро
    function xmlEscape($text)
    {
        return htmlspecialchars($text, ENT_QUOTES | ENT_XML1, 'UTF-8');
    }


    // Длина строки в символах UTF-8: без mbstring, только PCRE
    function utf8Length($text)
    {
        return preg_match_all('/./us', $text);
    }

} // Закончился класс BaseGenerator


function delDir($dir)
{
    if(!is_dir($dir))
        return;

    $files = array_diff(scandir($dir), array('.','..'));
    foreach ($files as $file) {
        (is_dir("$dir/$file")) ? delDir("$dir/$file") : unlink("$dir/$file");
    }

    rmdir($dir);

    return;
}


function getId()
{
    $result = '';
    $array = array_merge(range('a','z'), range('0','9'));

    for($i=0; $i<10; $i++){
        $result .= $array[mt_rand(0, 35)];
    }

    $result=((string) time()).$result;

    return $result;
}

?>

#!/usr/bin/php-cgi
<?php
require( 'header.php' );
?>

<div class="container-background">
<div class="container">
<div class="result-list">

<?php
// $myquery contains the query from the search engine
$myquery ="none";
if( !empty( $_GET['q'] ) )
    $myquery =$_GET['q'];
else if( !empty( $_POST['q'] ) )
    $myquery =$_POST['q'];

// $type contains either 'web' or 'images'
$type ='web';
if( !empty( $_POST['type'] ) )
    $type =$_POST['type'];
if( isset( $_GET['color'] ) )
        $type ="color";

// Write the query terms to a local file to minimize security problems
$handle  = fopen("../zoekmuis/queryterms.txt","w");
fwrite($handle,$myquery);
fclose($handle);

$commandstring = "(cd ../zoekmuis && ./webquery --$type)";

//Using backticks one way for PHP to call an external program and return the output
$myoutput =`$commandstring`;

echo $myoutput;
?>

</div></div></div>
</body></html>


    char tmp[32];
    queue_t q;
    queue_create( &q, 20, '\n' );
    while( 1 ) {
        printf( "> " );
        scanf( "%s", tmp );
        if( tmp[0] == '.' ) {
            if( queue_getCurrent( &q, &tmp[0], 32 ) == 0 ) {
                printf( "\tNothing to pop!" );
            }
            else {
                queue_pop( &q );
                printf( "\tPopped '%s'", tmp );
            }
        }
        else if( tmp[0] == '*' ) {
            printf( "%s", q.data );
        }
        else if( tmp[0] == '[' ) {
            printf( "\t Front: %d", q.front );
        }
        else if( tmp[0] == ']' ) {
            printf( "\t Rear: %d", q.rear );
        }
        else {
            if( queue_push( &q, tmp, strlen( tmp ) ) != 0 )
                printf( "\t Queue full!" );
            else
                printf( "\t Pushed '%s'", tmp );
        }
        
        if( queue_getCurrent( &q, &tmp[0], 32 ) == 0 ) {
            printf( ", no current top.\n" );
        } else {
            printf( ", current top '%s'\n", tmp );
        }          
    }



    return 0;

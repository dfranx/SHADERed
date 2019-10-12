/*
  Native File Dialog

  http://www.frogtoss.com/labs
*/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "nfd.h"
#include "nfd_common.h"

#define SIMPLE_EXEC_IMPLEMENTATION
#include "simple_exec.h"


const char NO_ZENITY_MSG[] = "zenity not installed";


static void AddTypeToFilterName( const char *typebuf, char *filterName, size_t bufsize )
{
    size_t len = strlen(filterName);
    if( len > 0 )
        strncat( filterName, " *.", bufsize - len - 1 );
    else
        strncat( filterName, "--file-filter=*.", bufsize - len - 1 );
    
    len = strlen(filterName);
    strncat( filterName, typebuf, bufsize - len - 1 );
}

static void AddFiltersToCommandArgs(char** commandArgs, int commandArgsLen, const char *filterList )
{
    char typebuf[NFD_MAX_STRLEN] = {0};
    const char *p_filterList = filterList;
    char *p_typebuf = typebuf;
    char filterName[NFD_MAX_STRLEN] = {0};
    int i;
    
    if ( !filterList || strlen(filterList) == 0 )
        return;

    while ( 1 )
    {
        
        if ( NFDi_IsFilterSegmentChar(*p_filterList) )
        {
            char typebufWildcard[NFD_MAX_STRLEN];
            /* add another type to the filter */
            assert( strlen(typebuf) > 0 );
            assert( strlen(typebuf) < NFD_MAX_STRLEN-1 );
            
            snprintf( typebufWildcard, NFD_MAX_STRLEN, "*.%s", typebuf );

            AddTypeToFilterName( typebuf, filterName, NFD_MAX_STRLEN );
            
            p_typebuf = typebuf;
            memset( typebuf, 0, sizeof(char) * NFD_MAX_STRLEN );
        }
        
        if ( *p_filterList == ';' || *p_filterList == '\0' )
        {
            /* end of filter -- add it to the dialog */

            for(i = 0; commandArgs[i] != NULL && i < commandArgsLen; i++);

            commandArgs[i] = strdup(filterName);
            
            filterName[0] = '\0';

            if ( *p_filterList == '\0' )
                break;
        }

        if ( !NFDi_IsFilterSegmentChar( *p_filterList ) )
        {
            *p_typebuf = *p_filterList;
            p_typebuf++;
        }

        p_filterList++;
    }
    
    /* always append a wildcard option to the end*/
    
    for(i = 0; commandArgs[i] != NULL && i < commandArgsLen; i++);

    commandArgs[i] = strdup("--file-filter=*.*");
}

static nfdresult_t ZenityCommon(char** command, int commandLen, const char* defaultPath, const char* filterList, char** stdOut)
{
    if(defaultPath != NULL)
    {
        char* prefix = "--filename=";
        int len = strlen(prefix) + strlen(defaultPath) + 1;

        char* tmp = (char*) calloc(len, 1);
        strcat(tmp, prefix);
        strcat(tmp, defaultPath);

        int i;
        for(i = 0; command[i] != NULL && i < commandLen; i++);

        command[i] = tmp;
    }

    AddFiltersToCommandArgs(command, commandLen, filterList);

    int byteCount = 0;
    int exitCode = 0;
    int processInvokeError = runCommandArray(stdOut, &byteCount, &exitCode, 0, command);

    for(int i = 0; command[i] != NULL && i < commandLen; i++)
        free(command[i]);

    nfdresult_t result = NFD_OKAY;

    if(processInvokeError == COMMAND_NOT_FOUND)
    {
        NFDi_SetError(NO_ZENITY_MSG);
        result = NFD_ERROR;
    }
    else
    {
        if(exitCode == 1)
            result = NFD_CANCEL;
    }

    return result;
}
 

static nfdresult_t AllocPathSet(char* zenityList, nfdpathset_t *pathSet )
{
    assert(zenityList);
    assert(pathSet);
    
    size_t len = strlen(zenityList) + 1;
    pathSet->buf = NFDi_Malloc(len);

    int numEntries = 1;

    for(size_t i = 0; i < len; i++)
    {
        char ch = zenityList[i];

        if(ch == '|')
        {
            numEntries++;
            ch = '\0';
        }

        pathSet->buf[i] = ch;
    }

    pathSet->count = numEntries;
    assert( pathSet->count > 0 );

    pathSet->indices = NFDi_Malloc( sizeof(size_t)*pathSet->count );

    int entry = 0;
    pathSet->indices[0] = 0;
    for(size_t i = 0; i < len; i++)
    {
        char ch = zenityList[i];

        if(ch == '|')
        {
            entry++;
            pathSet->indices[entry] = i + 1;
        }
    }
    
    return NFD_OKAY;
}
                                 
/* public */

nfdresult_t NFD_OpenDialog( const char *filterList,
                            const nfdchar_t *defaultPath,
                            nfdchar_t **outPath )
{    
    int commandLen = 100;
    char* command[commandLen];
    memset(command, 0, commandLen * sizeof(char*));

    command[0] = strdup("zenity");
    command[1] = strdup("--file-selection");
    command[2] = strdup("--title=Open File");

    char* stdOut = NULL;
    nfdresult_t result = ZenityCommon(command, commandLen, defaultPath, filterList, &stdOut);
            
    if(stdOut != NULL)
    {
        size_t len = strlen(stdOut);
        *outPath = NFDi_Malloc(len);
        memcpy(*outPath, stdOut, len);
        (*outPath)[len-1] = '\0'; // trim out the final \n with a null terminator
        free(stdOut);
    }
    else
    {
        *outPath = NULL;
    }

    return result;
}


nfdresult_t NFD_OpenDialogMultiple( const nfdchar_t *filterList,
                                    const nfdchar_t *defaultPath,
                                    nfdpathset_t *outPaths )
{
    int commandLen = 100;
    char* command[commandLen];
    memset(command, 0, commandLen * sizeof(char*));

    command[0] = strdup("zenity");
    command[1] = strdup("--file-selection");
    command[2] = strdup("--title=Open Files");
    command[3] = strdup("--multiple");

    char* stdOut = NULL;
    nfdresult_t result = ZenityCommon(command, commandLen, defaultPath, filterList, &stdOut);
            
    if(stdOut != NULL)
    {
        size_t len = strlen(stdOut);
        stdOut[len-1] = '\0'; // remove trailing newline

        if ( AllocPathSet( stdOut, outPaths ) == NFD_ERROR )
            result = NFD_ERROR;

        free(stdOut);
    }
    else
    {
        result = NFD_ERROR;
    }

    return result;
}

nfdresult_t NFD_SaveDialog( const nfdchar_t *filterList,
                            const nfdchar_t *defaultPath,
                            nfdchar_t **outPath )
{
    int commandLen = 100;
    char* command[commandLen];
    memset(command, 0, commandLen * sizeof(char*));

    command[0] = strdup("zenity");
    command[1] = strdup("--file-selection");
    command[2] = strdup("--title=Save File");
    command[3] = strdup("--save");

    char* stdOut = NULL;
    nfdresult_t result = ZenityCommon(command, commandLen, defaultPath, filterList, &stdOut);
            
    if(stdOut != NULL)
    {
        size_t len = strlen(stdOut);
        *outPath = NFDi_Malloc(len);
        memcpy(*outPath, stdOut, len);
        (*outPath)[len-1] = '\0'; // trim out the final \n with a null terminator
        free(stdOut);
    }
    else
    {
        *outPath = NULL;
    }

    return result;
}

nfdresult_t NFD_PickFolder(const nfdchar_t *defaultPath,
    nfdchar_t **outPath)
{
    int commandLen = 100;
    char* command[commandLen];
    memset(command, 0, commandLen * sizeof(char*));

    command[0] = strdup("zenity");
    command[1] = strdup("--file-selection");
    command[2] = strdup("--directory");
    command[3] = strdup("--title=Select folder");

    char* stdOut = NULL;
    nfdresult_t result = ZenityCommon(command, commandLen, defaultPath, "", &stdOut);
            
    if(stdOut != NULL)
    {
        size_t len = strlen(stdOut);
        *outPath = NFDi_Malloc(len);
        memcpy(*outPath, stdOut, len);
        (*outPath)[len-1] = '\0'; // trim out the final \n with a null terminator
        free(stdOut);
    }
    else
    {
        *outPath = NULL;
    }

    return result;
}

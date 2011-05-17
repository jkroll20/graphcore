#ifndef CLIBASE_H
#define CLIBASE_H

#ifndef _
#define _(string) gettext(string)
#endif


// convenience macros for printing success/failure/error messages from the cli.
// if the protocol for those messages should ever have to be modified, change these.
#define SUCCESS_STR "OK."
#define FAIL_STR "FAILED!"
#define ERROR_STR "ERROR!"
#define NONE_STR "NONE."
#define cliMessage(str, x...) ({ char c[2048]; int n= sprintf(c, str " "); snprintf(c+n, sizeof(c)-n, x); lastStatusMessage= c; })
#define cliSuccess(x...) cliMessage(SUCCESS_STR, x)
#define cliFailure(x...) cliMessage(FAIL_STR, x)
#define cliError(x...) cliMessage(ERROR_STR, x)
#define cliNone(x...) cliMessage(NONE_STR, x)

enum CommandStatus
{
    CMD_SUCCESS= 0,
    CMD_FAILURE,
    CMD_ERROR,
    CMD_NONE,
};



// abstract base class for cli commands
class CliCommand
{
    public:
        enum ReturnType
        {
            RT_NONE,
            RT_ARC_LIST,
            RT_NODE_LIST,
            RT_OTHER,
        };

        CliCommand() { }
        virtual ~CliCommand() { }

        // the command name
        virtual std::string getName()            { return "CliCommand"; }
        // one line describing the command and its parameters
        virtual std::string getSynopsis()        { return getName(); }
        // help text describing the function of the command
        virtual std::string getHelpText()        { return "Help text for " + getName() + "."; }
        void syntaxError()
        {
            lastStatusMessage= std::string(FAIL_STR) + _(" Syntax: ") + getSynopsis() + "\n";
            if(getReturnType()==RT_OTHER) std::cout << lastStatusMessage;
        }
        const std::string &getStatusMessage()    { return lastStatusMessage; }
        virtual ReturnType getReturnType()= 0;
        // read a data set of node IDs.
        // expectedSize: expected size of set per line (e. g. 1 for nodes, 2 for arcs)
        // update lastErrorString and return true on success, false on failure.
        bool readNodeset(FILE *inFile, std::vector< std::vector<uint32_t> > &dataset, unsigned expectedSize);


    protected:
        std::string lastStatusMessage;
};



class Cli
{
    public:
        Cli(): doQuit(false) {}

        ~Cli()
        {
            for(unsigned i= 0; i<commands.size(); i++)
                delete(commands[i]);
            commands.clear();
        }

        CliCommand *findCommand(string name)
        {
            for(unsigned i= 0; i<commands.size(); i++)
                if(commands[i]->getName()==name) return commands[i];
            return 0;
        }

        vector<CliCommand*> &getCommands() { return commands; }

        void quit() { doQuit= true; }

        // convert string to unsigned int
        static uint32_t parseUint(string str)
        {
            return strtoul(str.c_str(), 0, 0);
        }

        // check if string forms a valid unsigned integer
        static bool isValidUint(const string& s)
        {
            // disallow empty strings
            if(s.length()<1) return false;
            // allow only positive decimal digits
            for(size_t i= 0; i<s.length(); i++)
                if( !isdigit(s[i]) ) return false;
            return true;
        }

        // check if string forms a valid node (vertex) id.
        static bool isValidNodeID(const string& s)
        {
            return isValidUint(s) && parseUint(s)!=0;
        }

        // parse a data record in text form, check for valid uints
        static bool readUintRecord(FILE *f, vector<uint32_t> &ret)
        {
            char line[1024];
            uint32_t n;
            if(fgets(line, 1024, f)==0)
            {
                if(!feof(f)) return false;
                else return true;
            }
            if( (n= strlen(line)) && line[n-1]=='\n' ) line[--n]= 0;    // chomp line buffer.
            vector<string> strings= splitString(line);
            if(strlen(line) && !strings.size())
                // a non-empty string with no words (i. e. entirely made up of delimiters) is illegal
                return false;
            for(uint32_t i= 0; i<strings.size(); i++)
            {
                if(!isValidUint(strings[i])) return false;
                ret.push_back(parseUint(strings[i]));
            }
            return true;
        }

        // like readUintRecord(), and also check that integers are in valid range for node IDs (currently 1..uint32_max)
        static bool readNodeIDRecord(FILE *f, vector<uint32_t> &ret)
        {
            if(!readUintRecord(f, ret)) return false;
            for(vector<uint32_t>::iterator i= ret.begin(); i!=ret.end(); i++)
                if(*i==0) return false;
            return true;
        }


    protected:
        bool doQuit;

        vector<CliCommand*> commands;

        // read a line from stdin.
        // return 0 on error
        virtual char *getLine()
        {
            char *linebuf= (char*)malloc(1024);
            if(!linebuf) return 0;
            char *l= fgets(linebuf, 1024, stdin);
            if(!l) free(linebuf);
            return l;
        }


        // get i/o redirection filename from command line
        static char *getRedirFilename(char *str)
        {
            while(isspace(*str)) str++;
            char *s= str;
            while(*s && !isspace(*s)) s++;
            *s= 0;
            return str;
        }

        // split a string into words using given delimiters
        static vector<string> splitString(const char *s, const char *delim= " \n\t,")
        {
            vector<string> ret;
            const char *str= s;
            while(true)
            {
                while(*str && strchr(delim, *str)) str++;
                if(!*str) return ret;
                const char *start= str;
                while(*str && !strchr(delim, *str)) str++;
                ret.push_back(string(start, str-start));
                if(*str) str++;
            }
        }
};



inline bool CliCommand::readNodeset(FILE *inFile, vector< vector<uint32_t> > &dataset, unsigned expectedSize)
{
    vector<uint32_t> record;
    bool ok= true;
    cliSuccess("\n");
    for(unsigned lineno= 1; ; lineno++)
    {
        record.clear();
        if( !Cli::readNodeIDRecord(inFile, record) )
        {
            if(ok) cliError(_("error reading data set (line %u)\n"), lineno);
            ok= false;
        }
        else if(record.size()==0)
        {
            return ok;
        }
        else if(record.size()!=expectedSize)
        {
            if(ok) cliError(_("error reading data set (line %u)\n"), lineno);
            ok= false;
        }
        else
        {
            // unnecessary leftover, now done in readNodeIDRecord.
//            if(record[0]==0 || record[1]==0) { cliError(_("invalid node ID in line %d\n"), lineno); ok= false; }
            if(ok) dataset.push_back(record);
        }
    }
}







#endif // CLIBASE_H






#ifndef ASCII_FILE_H
#define ASCII_FILE_H


#include <string>
#include <vector>


namespace storage
{
    using std::string;
    using std::vector;


    class Regex;


    class AsciiFile
    {
    public:

	AsciiFile( bool CreateBackup_bv=false,
		   const char* BackupExt_Cv=".orig" );
	AsciiFile( const char* Name_Cv, bool CreateBackup_bv=false,
		   const char* BackupExt_Cv=".orig" );
	AsciiFile( const string& Name_Cv, bool CreateBackup_bv=false,
		   const char* BackupExt_Cv=".orig" );
	~AsciiFile();

	bool insertFile( AsciiFile& File_Cv, unsigned int BeforeLine_iv=0 );
	bool appendFile( AsciiFile& File_Cv );
	bool insertFile( const string& Name_Cv, unsigned int BeforeLine_iv=0 );
	bool appendFile( const string& Name_Cv );

	string fileName() const { return Name_C; }
	bool loadFile( const string& Name_Cv );
	bool updateFile();
	bool saveToFile( const string& Name_Cv );

	void append( const string& Line_Cv );
	void append( const vector<string>& Lines_Cv );
	void insert( unsigned int Before_iv, const string& Line_Cv );
	void remove( unsigned int Start_iv, unsigned int Cnt_iv );
	void replace( unsigned int Start_iv, unsigned int Cnt_iv,
		      const string& Line_Cv );
	void replace( unsigned int Start_iv, unsigned int Cnt_iv,
		      const vector<string>& Line_Cv );

	const string& operator []( unsigned int Index_iv ) const;
	string& operator []( unsigned int Index_iv );

	int find( unsigned int Start_iv, const string& Pat_Cv ) const;
	int find( unsigned int Start_iv, Regex& Pat_Cv ) const;

	unsigned numLines() const { return Lines_C.size(); }

	bool removeIfEmpty() const;

	const vector<string>& lines() const { return Lines_C; }

    protected:

	bool appendFile( const string& Name_Cv, vector<string>& Lines_Cr ) const;
	bool appendFile( AsciiFile& File_Cv, vector<string>& Lines_Cr ) const;
	void removeLastIf(string& Text_Cr, char Char_cv) const;

	bool BackupCreated_b;
	string BackupExtension_C;
	vector<string> Lines_C;
	string Name_C;

    };

}

#endif

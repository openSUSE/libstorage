// -*- C++ -*-
// Maintainer: fehr@suse.de

#ifndef _AsciiFile_h
#define _AsciiFile_h

#include <vector>

using std::vector;


#define DBG(x)

///////////////////////////////////////////////////////////////////
//
//	CLASS NAME : AsciiFile
//
//	DESCRIPTION :
//
class AsciiFile
    {
    public:
	AsciiFile( bool CreateBackup_bv=false, 
		   const char* BackupExt_Cv=".orig" );
	AsciiFile( const string& Name_Cv, bool CreateBackup_bv=false, 
		   const char* BackupExt_Cv=".orig" );
	AsciiFile( const char* Name_Cv, bool CreateBackup_bv=false, 
		   const char* BackupExt_Cv=".orig" );
	~AsciiFile();
	bool insertFile( AsciiFile& File_Cv, unsigned int BeforeLine_iv=0 );
	bool appendFile( AsciiFile& File_Cv );
	bool insertFile( string Name_Cv, unsigned int BeforeLine_iv=0 );
	bool appendFile( string Name_Cv );
	bool loadFile( string Name_Cv );
	bool updateFile();
	bool saveToFile( string Name_Cv );
	void append( string Line_Cv );
	void insert( unsigned int Before_iv, string Line_Cv );
	void remove( unsigned int Start_iv, unsigned int Cnt_iv );
	void replace( unsigned int Start_iv, unsigned int Cnt_iv,
		      string Line_Cv );
	const string& operator []( unsigned int Index_iv ) const;
	string& operator []( unsigned int Index_iv );
	int find( unsigned int Start_iv, string Pat_Cv );
	int numLines() const; 
	string fileName();
	int differentLine( const AsciiFile& File_Cv ) const;

    protected:
	bool appendFile( string Name_Cv, vector<string>& Lines_Cr );
	bool appendFile( AsciiFile& File_Cv, vector<string>& Lines_Cr );

	bool BackupCreated_b;
	string BackupExtension_C;
	vector<string> Lines_C;
	string Name_C;
    };
///////////////////////////////////////////////////////////////////

#endif

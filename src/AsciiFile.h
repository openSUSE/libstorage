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
	bool InsertFile( AsciiFile& File_Cv, unsigned int BeforeLine_iv=0 );
	bool AppendFile( AsciiFile& File_Cv );
	bool InsertFile( string Name_Cv, unsigned int BeforeLine_iv=0 );
	bool AppendFile( string Name_Cv );
	bool LoadFile( string Name_Cv );
	bool UpdateFile();
	bool SaveToFile( string Name_Cv );
	void Append( string Line_Cv );
	void Insert( unsigned int Before_iv, string Line_Cv );
	void Delete( unsigned int Start_iv, unsigned int Cnt_iv );
	void Replace( unsigned int Start_iv, unsigned int Cnt_iv,
		      string Line_Cv );
	const string& operator []( unsigned int Index_iv ) const;
	string& operator []( unsigned int Index_iv );
	int Find( unsigned int Start_iv, string Pat_Cv );
	int NumLines() const; 
	string FileName();
	int DifferentLine( const AsciiFile& File_Cv ) const;

    protected:
	bool AppendFile( string Name_Cv, vector<string>& Lines_Cr );
	bool AppendFile( AsciiFile& File_Cv, vector<string>& Lines_Cr );

	bool BackupCreated_b;
	string BackupExtension_C;
	vector<string> Lines_C;
	string Name_C;
    };
///////////////////////////////////////////////////////////////////

#endif

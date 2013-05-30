#include "Table.h"

TInt const TTable::Last =-1;
TInt const TTable::Invalid =-2;


void TTable::TRowIteratorWithRemove::RemoveNext(){
  TInt OldNextRowIdx = GetNextRowIdx();
  if(OldNextRowIdx == Table->FirstValidRow){
    Table->RemoveFirstRow();
    return;
  }
  Assert(OldNextRowIdx != Invalid);
  if(OldNextRowIdx == Last){ return;}
  Table->Next[CurrRowIdx] = Table->Next[OldNextRowIdx];
  Table->Next[OldNextRowIdx] = Invalid;
  Table->NumValidRows--;
}

TTable::TTable(const TStr& TableName, const Schema& TableSchema): Name(TableName),
  S(TableSchema), NumRows(0), NumValidRows(0), FirstValidRow(0), NumOfDistinctStrVals(0){
  TInt IntColCnt = 0;
  TInt FltColCnt = 0;
  TInt StrColCnt = 0;
  for(TInt i = 0; i < S.Len(); i++){
    TStr ColName = GetSchemaColName(i);
    TYPE ColType = GetSchemaColType(i);
    switch(ColType){
      case INT:
        ColTypeMap.AddDat(ColName, TPair<TYPE,TInt>(INT, IntColCnt));
        IntColCnt++;
        break;
      case FLT:
        ColTypeMap.AddDat(ColName, TPair<TYPE,TInt>(FLT, FltColCnt));
        FltColCnt++;
        break;
      case STR:
        ColTypeMap.AddDat(ColName, TPair<TYPE,TInt>(STR, StrColCnt));
        StrColCnt++;
        break;
    }
  }
  IntCols = TVec<TIntV>(IntColCnt);
  FltCols = TVec<TFltV>(FltColCnt);
  StrColMaps = TVec<TIntV>(StrColCnt);
}

PTable TTable::LoadSS(const TStr& TableName, const Schema& S, const TStr& InFNm, const TIntV& RelevantCols, const char& Separator, TBool HasTitleLine){
  TSsParser Ss(InFNm, Separator);
  Schema SR;
  if(RelevantCols.Len() == 0){
    SR = S;
  } else{
    for(TInt i = 0; i < RelevantCols.Len(); i++){
      SR.Add(S[RelevantCols[i]]);
    }
  }
  PTable T = New(TableName, SR);

  // if title line (i.e. names of the columns) is included as first row in the
  // input file - use it to validate schema
  if(HasTitleLine){
    Ss.Next();  
    if(S.Len() != Ss.GetFlds()){TExcept::Throw("Table Schema Mismatch!");}
    for(TInt i = 0; i < Ss.GetFlds(); i++){
      // remove carriage return char
      TInt L = strlen(Ss[i]);
      if(Ss[i][L-1] < ' '){ Ss[i][L-1] = 0;}
      if(S[i].Val1 != Ss[i]){ TExcept::Throw("Table Schema Mismatch!");}
    }
  }

  TInt RowLen = SR.Len();
  TVec<TYPE> ColTypes = TVec<TYPE>(RowLen);
  for(TInt i = 0; i < RowLen; i++){
    ColTypes[i] = T->GetSchemaColType(i);
  }
  // populate table columns
  while(Ss.Next()){
    TInt IntColIdx = 0;
    TInt FltColIdx = 0;
    TInt StrColIdx = 0;
    Assert(Ss.GetFlds() == S.Len()); // compiled only in debug
    for(TInt i = 0; i < RowLen; i++){
      switch(ColTypes[i]){
        case INT:
          if(RelevantCols.Len() == 0){
            T->IntCols[IntColIdx].Add(Ss.GetInt(i));
          } else {
            T->IntCols[IntColIdx].Add(Ss.GetInt(RelevantCols[i]));
          }
          IntColIdx++;
          break;
        case FLT:
          if(RelevantCols.Len() == 0){
            T->FltCols[FltColIdx].Add(Ss.GetFlt(i));
          } else {
            T->FltCols[FltColIdx].Add(Ss.GetFlt(RelevantCols[i]));
          }
          FltColIdx++;
          break;
        case STR:
          if(RelevantCols.Len() == 0){
            T->AddStrVal(StrColIdx, Ss[i]);
          } else {
            T->AddStrVal(StrColIdx, Ss[RelevantCols[i]]);
          }
          StrColIdx++;
          break;
      }
    }
  }

  // set number of rows and "Next" vector
  T->NumRows = Ss.GetLineNo()-1;
  if(HasTitleLine){T->NumRows--;}
  T->NumValidRows = T->NumRows;
  T->Next = TIntV(T->NumRows,0);
  for(TInt i = 0; i < T->NumRows-1; i++){
    T->Next.Add(i+1);
  }
  T->Next.Add(Last);
  return T;
}

PTable TTable::LoadSS(const TStr& TableName, const Schema& S, const TStr& InFNm, const char& Separator, TBool HasTitleLine){
  return LoadSS(TableName, S, InFNm, TIntV(), Separator, HasTitleLine);
}


void TTable::SaveSS(const TStr& OutFNm){
  FILE* F = fopen(OutFNm.CStr(), "w");
  // debug
  if(F == NULL){
    printf("failed to open file %s\n", OutFNm.CStr());
    perror("fail ");
    return;
  }

  TInt L = S.Len();
  // print title (schema)
  for(TInt i = 0; i < L-1; i++){
    fprintf(F, "%s\t", GetSchemaColName(i).CStr());
  }  
   fprintf(F, "%s\n", GetSchemaColName(L-1).CStr());
  // print table contents
  for(TRowIterator RowI = BegRI(); RowI < EndRI(); RowI++){
    for(TInt i = 0; i < L; i++){
      char C = (i == L-1) ? '\n' : '\t';
	    switch(GetSchemaColType(i)){
	      case INT:{
		    fprintf(F, "%d%c", RowI.GetIntAttr(GetSchemaColName(i)).Val, C);
		    break;
		  }
	     case FLT:{
		    fprintf(F, "%f%c", RowI.GetFltAttr(GetSchemaColName(i)).Val, C);
		    break;
		  }
	    case STR:{
		    fprintf(F, "%s%c", RowI.GetStrAttr(GetSchemaColName(i)).CStr(), C);
		    break;
		  }
	   }
	}
 }
 fclose(F);
}

// expensive - consider using a StrHash / hashSet for str vals
// to verify uniquness of str vals
void TTable::AddStrVal(TInt ColIdx, TStr Val){
  for(TInt i = 0; i < NumOfDistinctStrVals; i++){
    if(Val == StrColVals.GetCStr(i)){
      StrColMaps[ColIdx].Add(i);
      return;
    }
  }
  TInt K = StrColVals.AddStr(Val);
  StrColMaps[ColIdx].Add(K);
  NumOfDistinctStrVals++;
}

void TTable::AddStrVal(TStr Col, TStr Val){
  if(GetColType(Col) != STR){TExcept::Throw(Col + " is not a string valued column");}
  AddStrVal(GetColIdx(Col), Val);
}

TStrV TTable::GetSrcNodeIntAttrV() const {
  TStrV IntNA = TStrV(IntCols.Len(),0);
  for (int i = 0; i < SrcNodeAttrV.Len(); i++) {
    TStr Attr = SrcNodeAttrV[i];
    if (GetColType(Attr) == INT) {
      IntNA.Add(Attr);
    }
  }
  return IntNA;
}

TStrV TTable::GetDstNodeIntAttrV() const {
  TStrV IntNA = TStrV(IntCols.Len(),0);
  for (int i = 0; i < DstNodeAttrV.Len(); i++) {
    TStr Attr = DstNodeAttrV[i];
    if (GetColType(Attr) == INT) {
      IntNA.Add(Attr);
    }
  }
  return IntNA;
}

TStrV TTable::GetEdgeIntAttrV() const {
  TStrV IntEA = TStrV(IntCols.Len(),0);
  for (int i = 0; i < EdgeAttrV.Len(); i++) {
    TStr Attr = EdgeAttrV[i];
    if (GetColType(Attr) == INT) {
      IntEA.Add(Attr);
    }
  }
  return IntEA;
}

TStrV TTable::GetSrcNodeFltAttrV() const {
  TStrV FltNA = TStrV(FltCols.Len(),0);
  for (int i = 0; i < SrcNodeAttrV.Len(); i++) {
    TStr Attr = SrcNodeAttrV[i];
    if (GetColType(Attr) == FLT) {
      FltNA.Add(Attr);
    }
  }
  return FltNA;
}

TStrV TTable::GetDstNodeFltAttrV() const {
  TStrV FltNA = TStrV(FltCols.Len(),0);
  for (int i = 0; i < DstNodeAttrV.Len(); i++) {
    TStr Attr = DstNodeAttrV[i];
    if (GetColType(Attr) == FLT) {
      FltNA.Add(Attr);
    }
  }
  return FltNA;
}

TStrV TTable::GetEdgeFltAttrV() const {
  TStrV FltEA = TStrV(FltCols.Len(),0);;
  for (int i = 0; i < EdgeAttrV.Len(); i++) {
    TStr Attr = EdgeAttrV[i];
    if (GetColType(Attr) == FLT) {
      FltEA.Add(Attr);
    }
  }
  return FltEA;
}

TStrV TTable::GetSrcNodeStrAttrV() const {
  TStrV StrNA = TStrV(StrColMaps.Len(),0);
  for (int i = 0; i < SrcNodeAttrV.Len(); i++) {
    TStr Attr = SrcNodeAttrV[i];
    if (GetColType(Attr) == STR) {
      StrNA.Add(Attr);
    }
  }
  return StrNA;
}

TStrV TTable::GetDstNodeStrAttrV() const {
  TStrV StrNA = TStrV(StrColMaps.Len(),0);
  for (int i = 0; i < DstNodeAttrV.Len(); i++) {
    TStr Attr = DstNodeAttrV[i];
    if (GetColType(Attr) == STR) {
      StrNA.Add(Attr);
    }
  }
  return StrNA;
}


TStrV TTable::GetEdgeStrAttrV() const {
  TStrV StrEA = TStrV(StrColMaps.Len(),0);
  for (int i = 0; i < EdgeAttrV.Len(); i++) {
    TStr Attr = EdgeAttrV[i];
    if (GetColType(Attr) == STR) {
      StrEA.Add(Attr);
    }
  }
  return StrEA;
}

/*
void TTable::ChangeWorkingCol(TStr column){
  if(!ColTypeMap.IsKey(column)){TExcept::Throw("no such column " + column);}
  WorkingCol = column;
}
*/
void TTable::AddLabel(TStr column, TStr NewLabel){
  if(!ColTypeMap.IsKey(column)){TExcept::Throw("no such column " + column);}
  TPair<TYPE,TInt> ColVal = ColTypeMap.GetDat(column);
  ColTypeMap.AddDat(NewLabel,ColVal);
}

void TTable::RemoveFirstRow(){
  TInt Old = FirstValidRow;
  FirstValidRow = Next[FirstValidRow];
  Next[Old] = Invalid;
  NumValidRows--;
}

void TTable::RemoveRow(TInt RowIdx){
  if(Next[RowIdx] == Invalid){ return;} // row was already removed
  if(RowIdx == FirstValidRow){
    RemoveFirstRow();
  } else{
    TInt i = RowIdx-1;
    while(Next[i] != RowIdx){i--;}
    Next[i] = Next[RowIdx];
  }
  Next[RowIdx] = Invalid;
  NumValidRows--;
}

void TTable::RemoveRows(const TIntV& RemoveV){
  for(TInt i = 0; i < RemoveV.Len(); i++){RemoveRow(RemoveV[i]);}
}


// TODO: simplify using TRowIteratorWithRemove
void TTable::KeepSortedRows(const TIntV& KeepV){
  // need to remove first rows
  while(FirstValidRow != KeepV[0]){ RemoveRow(FirstValidRow);}
  // at this point we know the first row will stay - i.e. FirstValidRow == KeepV[0]
  TInt KeepIdx = 1;
  TRowIterator RowI = BegRI();
  while(RowI < EndRI()){
    if(KeepV.Len() > KeepIdx){
      if(KeepV[KeepIdx] == Next[RowI.GetRowIdx()]){
        KeepIdx++;
        RowI++;
      } else{
          RemoveRow(Next[RowI.GetRowIdx()]);
      }
    // covered all of KeepV. remove the rest of the rows
    // current RowI.CurrRowIdx is the last element of KeepV
    } else{
      while(Next[RowI.GetRowIdx()] != Last){
        RemoveRow(Next[RowI.GetRowIdx()]);
      }
      // removed the rest of the rows. increment RowI to EndRI
      RowI++;
    }
  }
}

/*void TTable::Unique(TStr Col){
  if(!ColTypeMap.IsKey(Col)){TExcept::Throw("no such column " + Col);}
  TIntV RemainingRows = TIntV(NumValidRows,0);
  // group by given column (keys) and keep only first row for each key
  switch(GetColType(Col)){
    case INT:{
      THash<TInt,TIntV> T;  // can't really estimate the size of T for constructor hinting
      GroupByIntCol(Col, T, TIntV(0), true);
      for(THash<TInt,TIntV>::TIter it = T.BegI(); it < T.EndI(); it++){
        RemainingRows.Add(it->Dat[0]);
      }
      break;
    }
    case FLT:{
      THash<TFlt,TIntV> T;
      GroupByFltCol(Col, T, TIntV(0), true);
      for(THash<TFlt,TIntV>::TIter it = T.BegI(); it < T.EndI(); it++){
        RemainingRows.Add(it->Dat[0]);
      }
      break;
    }
    case STR:{
      THash<TStr,TIntV> T;
      GroupByStrCol(Col, T, TIntV(0), true);
      for(THash<TStr,TIntV>::TIter it = T.BegI(); it < T.EndI(); it++){
        RemainingRows.Add(it->Dat[0]);
      }
      break;
    }
  }
  // with the current implementation of GroupByX, RemainingRows is sorted:
  // GroupByX returns a hash Table T:X-->TIntV. In the current implementation,
  // if key X1 appears before key X2 in T Then T(X1)[0] <= T(X2)[0]
  // Not sure if we could always make this assumption. Might want to remove this sorting..
  RemainingRows.Sort();
  KeepSortedRows(RemainingRows);
}*/

void TTable::Unique(TStr Col){
  // with the current implementation of GroupByX, RemainingRows is sorted:
  // GroupByX returns a hash Table T:X-->TIntV. In the current implementation,
  // if key X1 appears before key X2 in T Then T(X1)[0] <= T(X2)[0]
  // Not sure if we could always make this assumption. Might want to remove this sorting..
  THash<TInt,TIntV> grouping;
  TIntV RemainingRows = TIntV(NumValidRows,0);
  TStrV GroupBy;
  GroupBy.Add(Col);
  GroupAux(GroupBy, 0, grouping, TIntV(0), false);
  for(THash<TInt,TIntV>::TIter it = grouping.BegI(); it < grouping.EndI(); it++){
    RemainingRows.Add(it->Dat[0]);
  }

  RemainingRows.Sort();
  KeepSortedRows(RemainingRows);
}

/* 
Q: This approach of passing a hash table by reference and adding entries to it is common here.
But it also implies certain assumptions about the input - i.e. empty input table. 
Why PHash is never used? - why not let the function allocate the new table and return a smart ptr PHash..
*/
void TTable::GroupByIntCol(TStr GroupBy, THash<TInt,TIntV>& grouping, const TIntV& IndexSet, TBool All) const{
  if(!ColTypeMap.IsKey(GroupBy)){TExcept::Throw("no such column " + GroupBy);}
  if(GetColType(GroupBy) != INT){TExcept::Throw(GroupBy + " values are not of expected type integer");}
  if(All){
     // optimize for the common and most expensive case - itearte over only valid rows
    for(TRowIterator it = BegRI(); it < EndRI(); it++){
      UpdateGrouping<TInt>(grouping, it.GetIntAttr(GroupBy), it.GetRowIdx());
    }
  } else{
    // consider only rows in IndexSet
    for(TInt i = 0; i < IndexSet.Len(); i++){
      if(IsRowValid(IndexSet[i])){
        TInt RowIdx = IndexSet[i];
        const TIntV& Col = IntCols[GetColIdx(GroupBy)];       
        UpdateGrouping<TInt>(grouping, Col[RowIdx], RowIdx);
      }
    }
  }
}

void TTable::GroupByFltCol(TStr GroupBy, THash<TFlt,TIntV>& grouping, const TIntV& IndexSet, TBool All) const{
  if(!ColTypeMap.IsKey(GroupBy)){TExcept::Throw("no such column " + GroupBy);}
  if(GetColType(GroupBy) != FLT){TExcept::Throw(GroupBy + " values are not of expected type float");}
   if(All){
     // optimize for the common and most expensive case - itearte over only valid rows
    for(TRowIterator it = BegRI(); it < EndRI(); it++){
      UpdateGrouping<TFlt>(grouping, it.GetFltAttr(GroupBy), it.GetRowIdx());
    }
  } else{
    // consider only rows in IndexSet
    for(TInt i = 0; i < IndexSet.Len(); i++){
      if(IsRowValid(IndexSet[i])){
        TInt RowIdx = IndexSet[i];
        const TFltV& Col = FltCols[GetColIdx(GroupBy)];       
        UpdateGrouping<TFlt>(grouping, Col[RowIdx], RowIdx);
      }
    }
  }
}

void TTable::GroupByStrCol(TStr GroupBy, THash<TStr,TIntV>& Grouping, const TIntV& IndexSet, TBool All) const{
  if(!ColTypeMap.IsKey(GroupBy)){TExcept::Throw("no such column " + GroupBy);}
  if(GetColType(GroupBy) != STR){TExcept::Throw(GroupBy + " values are not of expected type string");}
   if(All){
     // optimize for the common and most expensive case - itearte over all valid rows
    for(TRowIterator it = BegRI(); it < EndRI(); it++){
      UpdateGrouping<TStr>(Grouping, it.GetStrAttr(GroupBy), it.GetRowIdx());
    }
  } else{
    // consider only rows in IndexSet
    for(TInt i = 0; i < IndexSet.Len(); i++){
      if(IsRowValid(IndexSet[i])){
        TInt RowIdx = IndexSet[i];     
        UpdateGrouping<TStr>(Grouping, GetStrVal(GroupBy, RowIdx), RowIdx);
      }
    }
  }
}

/*
void TTable::GroupAux(const TStrV& GroupBy, TInt GroupByStartIdx, THash<TInt,TIntV>& grouping, const TIntV& IndexSet, TBool All){
  // recursion base - add IndexSet as group 
  if(GroupByStartIdx == GroupBy.Len()){
    if(IndexSet.Len() == 0){return;}
	  TInt key = grouping.Len();
	  grouping.AddDat(key, IndexSet);
	  return;
  }
  if(!ColTypeMap.IsKey(GroupBy[GroupByStartIdx])){TExcept::Throw("no such column " + GroupBy[GroupByStartIdx]);}
  switch(GetColType(GroupBy[GroupByStartIdx])){
    case INT:{
      // group by current column
      // not sure of to estimate the size of T for constructor hinting purpose.
      // It is bounded by the length of the IndexSet or the length of the grouping column if the IndexSet vector is empty
      // but this bound may be way too big
	    THash<TInt,TIntV> T;  
	    GroupByIntCol(GroupBy[GroupByStartIdx], T, IndexSet, All);
	    for(THash<TInt,TIntV>::TIter it = T.BegI(); it < T.EndI(); it++){
	      TIntV& CurrGroup = it->Dat;
        // each group according to current column will be used as an IndexSet
        // for grouping according to next column
		    GroupAux(GroupBy, GroupByStartIdx+1, grouping, CurrGroup, false);
	   }
	    break;
	  }
	  case FLT:{
	    THash<TFlt,TIntV> T;
	    GroupByFltCol(GroupBy[GroupByStartIdx], T, IndexSet, All);
	    for(THash<TFlt,TIntV>::TIter it = T.BegI(); it < T.EndI(); it++){
	      TIntV& CurrGroup = it->Dat;
		    GroupAux(GroupBy, GroupByStartIdx+1, grouping, CurrGroup, false);
	    }
	    break;
	  }
	  case STR:{
	    THash<TStr,TIntV> T;
	    GroupByStrCol(GroupBy[GroupByStartIdx], T, IndexSet, All);
	    for(THash<TStr,TIntV>::TIter it = T.BegI(); it < T.EndI(); it++){
	      TIntV& CurrGroup = it->Dat;
	      GroupAux(GroupBy, GroupByStartIdx+1, grouping, CurrGroup, false);
	    }
	    break;
	  }
  }
}
*/

void TTable::GroupAux(const TStrV& GroupBy, TInt GroupByStartIdx, THash<TInt,TIntV>& grouping, const TIntV& IndexSet, TBool InSet){

  THash<TIntV,TIntV> IGroup;
  THash<TFltV,TIntV> FGroup;
  THash<TStrV,TIntV> SGroup;
  TInt GroupNum = 0;
  for(TRowIterator it = BegRI(); it < EndRI(); it++){
    TIntV IKey;
    TFltV FKey;
    TStrV SKey;

    for (int i=0; i < GroupBy.Len(); i++) {
      if(!ColTypeMap.IsKey(GroupBy[i])){TExcept::Throw("no such column " + GroupBy[i]);}

      switch(GetColType(GroupBy[i])){
        case INT: {
          IKey.Add(it.GetIntAttr(GroupBy[i])); 
          break;
        }
        case FLT: {
          FKey.Add(it.GetFltAttr(GroupBy[i])); 
          break;
        }
        case STR: {
          SKey.Add(it.GetStrAttr(GroupBy[i])); 
          break;
        }
      }
    }

    TInt RowIdx = it.GetRowIdx();
    TIntV GroupMatch;
 
    if(IGroup.IsKey(IKey)){
      GroupMatch.Union(IGroup.GetDat(IKey));
      if(FGroup.IsKey(FKey)){
        GroupMatch.Intrs(FGroup.GetDat(FKey));
        if(SGroup.IsKey(SKey)){
          GroupMatch.Intrs(SGroup.GetDat(SKey));
        } else {
          GroupMatch.Clr();
        }
      } else {
        GroupMatch.Clr();
      }
    }

    if (GroupMatch.Len() == 0) {
      TIntV NewGroup;
      NewGroup.Add(RowIdx);
      grouping.AddDat(GroupNum, NewGroup);

      UpdateGrouping<TIntV>(IGroup, IKey, GroupNum);
      UpdateGrouping<TFltV>(FGroup, FKey, GroupNum);
      UpdateGrouping<TStrV>(SGroup, SKey, GroupNum);
      GroupNum++;
    } else if (GroupMatch.Len() == 1) {
      TInt CurrGroupNum = GroupMatch[0];
      grouping.GetDat(CurrGroupNum).Add(RowIdx);
    } else {
      TExcept::Throw("Groups duplicated, should not happen");
    }
  }
}

void TTable::StoreGroupCol(TStr GroupColName, const THash<TInt,TIntV>& Grouping){
  GroupMapping.AddDat(GroupColName, Grouping);
  // add a column where the value of the i'th row is the group id of row i
  IntCols.Add(TIntV(NumRows));
  TInt L = IntCols.Len();
  ColTypeMap.AddDat(GroupColName, TPair<TYPE,TInt>(INT, L-1));
  for(THash<TInt,TIntV>::TIter it = Grouping.BegI(); it < Grouping.EndI(); it++){
    TIntV& G = it->Dat;
    for(TInt i = 0; i < G.Len(); i++){
      IntCols[L-1][G[i]] = it->Key;
    }
  }
}

void TTable::Group(TStr GroupColName, const TStrV& GroupBy){
  THash<TInt,TIntV> grouping;
  GroupAux(GroupBy, 0, grouping, TIntV(0), false);
  StoreGroupCol(GroupColName, grouping);
  AddSchemaCol(GroupColName, INT); // update schema
}

void TTable::Count(TStr CountColName, TStr Col){
  if(!ColTypeMap.IsKey(Col)){TExcept::Throw("no such column " + Col);}
  TIntV CntCol(NumRows);
  switch(GetColType(Col)){
    case INT:{
      THash<TInt,TIntV> T;  // can't really estimate the size of T for constructor hinting
      TIntV& Column = IntCols[GetColIdx(Col)];
      GroupByIntCol(Col, T, TIntV(0), true);
      for(TRowIterator it = BegRI(); it < EndRI(); it++){
        CntCol[it.GetRowIdx()] = T.GetDat(Column[it.GetRowIdx()]).Len();
      }
      break;
    }
    case FLT:{
      THash<TFlt,TIntV> T;
      TFltV& Column = FltCols[GetColIdx(Col)];
      GroupByFltCol(Col, T, TIntV(0), true);
      for(TRowIterator it = BegRI(); it < EndRI(); it++){
         CntCol[it.GetRowIdx()] = T.GetDat(Column[it.GetRowIdx()]).Len();
      }
      break;
    }
    case STR:{
      THash<TStr,TIntV> T;
      GroupByStrCol(Col, T, TIntV(0), true);
      for(TRowIterator it = BegRI(); it < EndRI(); it++){
        CntCol[it.GetRowIdx()] = T.GetDat(GetStrVal(Col, it.GetRowIdx())).Len();
      }
    }
  }
  // add count column
  IntCols.Add(CntCol);
  AddSchemaCol(CountColName, INT);
  ColTypeMap.AddDat(CountColName, TPair<TYPE,TInt>(INT, IntCols.Len()-1));
}

 PTable TTable::InitializeJointTable(const TTable& Table){
  PTable JointTable = New();
  JointTable->Name = Name + "_" + Table.Name;
  JointTable->IntCols = TVec<TIntV>(IntCols.Len() + Table.IntCols.Len());
  JointTable->FltCols = TVec<TFltV>(FltCols.Len() + Table.FltCols.Len());
  JointTable->StrColMaps = TVec<TIntV>(StrColMaps.Len() + Table.StrColMaps.Len());
  for(TInt i = 0; i < S.Len(); i++){
    TStr ColName = GetSchemaColName(i);
    TYPE ColType = GetSchemaColType(i);
    TStr CName = Name + "." + ColName;
    JointTable->ColTypeMap.AddDat(CName, ColTypeMap.GetDat(ColName));
    JointTable->AddLabel(CName, ColName);
    JointTable->AddSchemaCol(CName, ColType);
  }
  for(TInt i = 0; i < Table.S.Len(); i++){
    TStr ColName = Table.GetSchemaColName(i);
    TYPE ColType = Table.GetSchemaColType(i);
    TStr CName = Table.Name + "." + ColName;
    TPair<TYPE, TInt> NewDat = Table.ColTypeMap.GetDat(ColName);
    Assert(ColType == NewDat.Val1);
    // add offsets
    switch(NewDat.Val1){
      case INT:
        NewDat.Val2 += IntCols.Len();
        break;
      case FLT:
        NewDat.Val2 += FltCols.Len();
        break;
      case STR:
        NewDat.Val2 += StrColMaps.Len();
        break;
    }
    JointTable->ColTypeMap.AddDat(CName, NewDat);
    JointTable->AddLabel(CName, ColName);
    JointTable->AddSchemaCol(CName, ColType);
  }
  return JointTable;
 }

 void TTable::AddJointRow(const TTable& T1, const TTable& T2, TInt RowIdx1, TInt RowIdx2){
   for(TInt i = 0; i < T1.IntCols.Len(); i++){
     IntCols[i].Add(T1.IntCols[i][RowIdx1]);
   }
   for(TInt i = 0; i < T1.FltCols.Len(); i++){
     FltCols[i].Add(T1.FltCols[i][RowIdx1]);
   }
   for(TInt i = 0; i < T1.StrColMaps.Len(); i++){
     AddStrVal(i, T1.GetStrVal(i, RowIdx1));
   }
   TInt IntOffset = T1.IntCols.Len();
   TInt FltOffset = T1.FltCols.Len();
   TInt StrOffset = T1.StrColMaps.Len();
   for(TInt i = 0; i < T2.IntCols.Len(); i++){
     IntCols[i+IntOffset].Add(T2.IntCols[i][RowIdx2]);
   }
   for(TInt i = 0; i < T2.FltCols.Len(); i++){
     FltCols[i+FltOffset].Add(T2.FltCols[i][RowIdx2]);
   }
   for(TInt i = 0; i < T2.StrColMaps.Len(); i++){
     AddStrVal(i+StrOffset, T2.GetStrVal(i, RowIdx2));
   }
   NumRows++;
   NumValidRows++;
   if(!Next.Empty()){ Next[Next.Len()-1] = NumValidRows-1;}
   Next.Add(Last);
 }

// Q: Do we want to have any gurantees in terms of order of the 0t rows - i.e. 
// ordered by "this" table row idx as primary key and "Table" row idx as secondary key
 // This means only keeping joint row indices (pairs of original row indices), sorting them
 // and adding all rows in the end. Sorting can be expensive, but we would be able to pre-allocate 
 // memory for the joint table..
PTable TTable::Join(TStr Col1, const TTable& Table, TStr Col2) {
  if(!ColTypeMap.IsKey(Col1)){
    TExcept::Throw("no such column " + Col1);
  }
  if(!ColTypeMap.IsKey(Col2)){
    TExcept::Throw("no such column " + Col2);
  }
  if (GetColType(Col1) != GetColType(Col2)) {
    TExcept::Throw("Trying to Join on columns of different type");
  }
  // initialize result table
  PTable JointTable = InitializeJointTable(Table);
  // hash smaller table (group by column)
  TYPE ColType = GetColType(Col1);
  TBool ThisIsSmaller = (NumValidRows <= Table.NumValidRows);
  const TTable& TS = ThisIsSmaller ? *this : Table;
  const TTable& TB = ThisIsSmaller ?  Table : *this;
  TStr ColS = ThisIsSmaller ? Col1 : Col2;
  TStr ColB = ThisIsSmaller ? Col2 : Col1;
  // iterate over the rows of the bigger table and check for "collisions" 
  // with the group keys for the small table.
  switch(ColType){
    case INT:{
      THash<TInt, TIntV> T;
      TS.GroupByIntCol(Col1, T, TIntV(), true);
      for(TRowIterator RowI = TB.BegRI(); RowI < TB.EndRI(); RowI++){
        TInt K = RowI.GetIntAttr(ColB);
        if(T.IsKey(K)){
          TIntV& Group = T.GetDat(K);
          for(TInt i = 0; i < Group.Len(); i++){
            if(ThisIsSmaller){
              JointTable->AddJointRow(*this, Table, Group[i], RowI.GetRowIdx());
            } else{
              JointTable->AddJointRow(*this, Table, RowI.GetRowIdx(), Group[i]);
            }
          }
        }
      }
      break;
    }
    case FLT:{
      THash<TFlt, TIntV> T;
      TS.GroupByFltCol(Col1, T, TIntV(), true);
      for(TRowIterator RowI = TB.BegRI(); RowI < TB.EndRI(); RowI++){
        TFlt K = RowI.GetFltAttr(ColB);
        if(T.IsKey(K)){
          TIntV& Group = T.GetDat(K);
          for(TInt i = 0; i < Group.Len(); i++){
            if(ThisIsSmaller){
              JointTable->AddJointRow(*this, Table, Group[i], RowI.GetRowIdx());
            } else{
              JointTable->AddJointRow(*this, Table, RowI.GetRowIdx(), Group[i]);
            }
          }
        }
      }
      break;
    }
    case STR:{
      THash<TStr, TIntV> T;
      TS.GroupByStrCol(Col1, T, TIntV(), true);
      for(TRowIterator RowI = TB.BegRI(); RowI < TB.EndRI(); RowI++){
        TStr K = RowI.GetStrAttr(ColB);
        if(T.IsKey(K)){
          TIntV& Group = T.GetDat(K);
          for(TInt i = 0; i < Group.Len(); i++){
            if(ThisIsSmaller){
              JointTable->AddJointRow(*this, Table, Group[i], RowI.GetRowIdx());
            } else{
              JointTable->AddJointRow(*this, Table, RowI.GetRowIdx(), Group[i]);
            }
          }
        }
      }
    }
    break;
  }
 return JointTable; 
}

void TTable::Dist(TStr Col1, const TTable& Table, TStr Col2, TStr DistColName, const TMetric& Metric, TFlt threshold) {
  if(!ColTypeMap.IsKey(Col1)){
    TExcept::Throw("no such column " + Col1);
  }
  if(!ColTypeMap.IsKey(Col2)){
    TExcept::Throw("no such column " + Col2);
  }
  //TYPE T1 = GetColType(Col1);
  //CHECK do we need to make this a const?
  /*
  TYPE T2 = Table.GetColType(Col2);
  if ((T1  == STR && T2 != STR) || (T1  != STR && T2 == STR) ) {
    TExcept::Throw("Trying to compare strings with numbers.");
  }
  */
}

void TTable::Select(TPredicate& Predicate){
  TIntV Selected;
  TStrV RelevantCols;
  Predicate.GetVariables(RelevantCols);
  TInt NumRelevantCols = RelevantCols.Len();
  TVec<TYPE> ColTypes = TVec<TYPE>(NumRelevantCols);
  TIntV ColIndices = TIntV(NumRelevantCols);
  for(TInt i = 0; i < NumRelevantCols; i++){
    ColTypes[i] = GetColType(RelevantCols[i]);
    ColIndices[i] = GetColIdx(RelevantCols[i]);
  } 
  
  TRowIteratorWithRemove RowI = BegRIWR();
  while(RowI.GetNextRowIdx() != Last){
    // prepare arguments for predicate evaluation
    for(TInt i = 0; i < NumRelevantCols; i++){
      switch(ColTypes[i]){
      case INT:
        Predicate.SetIntVal(RelevantCols[i], RowI.GetNextIntAttr(ColIndices[i]));
        break;
      case FLT:
        Predicate.SetFltVal(RelevantCols[i], RowI.GetNextFltAttr(ColIndices[i]));
        break;
      case STR:
        Predicate.SetStrVal(RelevantCols[i], RowI.GetNextStrAttr(ColIndices[i]));
        break;
      }
    }
    if(!Predicate.Eval()){ 
      RowI.RemoveNext();
    } else{
      RowI++;
    }
  }
}

void TTable::SelectAtomic(TStr Col1, TStr Col2, COMP Cmp){
  TYPE Ty1;
  TYPE Ty2;
  TInt ColIdx1;
  TInt ColIdx2;

  Ty1 = GetColType(Col1);
  Ty2 = GetColType(Col2);
  ColIdx1 = GetColIdx(Col1);
  ColIdx2 = GetColIdx(Col2);

  if(Ty1 != Ty2){
    TExcept::Throw("SelectAtomic: diff types");
  }

  TRowIteratorWithRemove RowI = BegRIWR();
  while(RowI.GetNextRowIdx() != Last){
    TBool Result;
    switch(Ty1){
      case INT:
        Result = EvalSelectAtomic(RowI.GetNextIntAttr(ColIdx1), RowI.GetNextIntAttr(ColIdx2), Cmp);
        break;
      case FLT:
        Result = EvalSelectAtomic(RowI.GetNextFltAttr(ColIdx1), RowI.GetNextFltAttr(ColIdx2), Cmp);
        break;
      case STR:
        Result = EvalSelectAtomic(RowI.GetNextStrAttr(ColIdx1), RowI.GetNextStrAttr(ColIdx2), Cmp);
        break;
    }
    if(!Result){ 
      RowI.RemoveNext();
    } else{
      RowI++;
    }
  }
}

void TTable::SelectAtomicIntConst(TStr Col1, TInt Val2, COMP Cmp){
  TYPE Ty1;
  TInt ColIdx1;

  Ty1 = GetColType(Col1);
  ColIdx1 = GetColIdx(Col1);
  if(Ty1 != INT){TExcept::Throw("SelectAtomic: not type TInt");}

  TRowIteratorWithRemove RowI = BegRIWR();
  while(RowI.GetNextRowIdx() != Last){
    if(!EvalSelectAtomic(RowI.GetNextIntAttr(ColIdx1), Val2, Cmp)){
      RowI.RemoveNext();
    } else{
      RowI++;
    }
  }
}

void TTable::SelectAtomicStrConst(TStr Col1, TStr Val2, COMP Cmp){
  TYPE Ty1;
  TInt ColIdx1;

  Ty1 = GetColType(Col1);
  ColIdx1 = GetColIdx(Col1);
  if(Ty1 != STR){TExcept::Throw("SelectAtomic: not type TStr");}

  TRowIteratorWithRemove RowI = BegRIWR();
  while(RowI.GetNextRowIdx() != Last){
    if(!EvalSelectAtomic(RowI.GetNextStrAttr(ColIdx1), Val2, Cmp)){
      RowI.RemoveNext();
    } else{
      RowI++;
    }
  }
}

void TTable::SelectAtomicFltConst(TStr Col1, TFlt Val2, COMP Cmp){
  TYPE Ty1;
  TInt ColIdx1;

  Ty1 = GetColType(Col1);
  ColIdx1 = GetColIdx(Col1);
  if(Ty1 != FLT){TExcept::Throw("SelectAtomic: not type TFlt");}

  TRowIteratorWithRemove RowI = BegRIWR();
  while(RowI.GetNextRowIdx() != Last){
    if(!EvalSelectAtomic(RowI.GetNextFltAttr(ColIdx1), Val2, Cmp)){
      RowI.RemoveNext();
    } else{
      RowI++;
    }
  }
}

void TTable::Defrag() {
  TInt FreeIndex = 0;
  TIntV Mapping;  // Mapping[old_index] = new_index/invalid
  for (TInt i = 0; i < Next.Len(); i++) {
    if (Next[i] != Invalid) {  
      // "first row" properly set beforehand
      if (FreeIndex == 0) {
        Assert (i == FirstValidRow);
        FirstValidRow = 0;
      }
 
      if (Next[i] != Last) { 
        Next[FreeIndex] = FreeIndex + 1;
        Mapping.Add(FreeIndex);
      } else {
        Next[FreeIndex] = Last;
        Mapping.Add(Last);
      }

      for (TInt j = 0; j < IntCols.Len(); j++) {
        IntCols[j][FreeIndex] = IntCols[j][i];
      }
      for (TInt j = 0; j < FltCols.Len(); j++) {
        FltCols[j][FreeIndex] = FltCols[j][i];
      }
      for (TInt j = 0; j < StrColMaps.Len(); j++) {
        StrColMaps[j][FreeIndex] = StrColMaps[j][i];
      }

      FreeIndex++;
    } else {
      NumRows--;
      Mapping.Add(Invalid);
    }
  }

  for(THash<TStr,THash<TInt,TIntV> >::TIter it = GroupMapping.BegI(); it < GroupMapping.EndI(); it++){
    THash<TInt,TIntV>& G = it->Dat;
    for(THash<TInt,TIntV>::TIter iit = G.BegI(); iit < G.EndI(); iit++) {
      TIntV& Group = iit->Dat;
      TInt FreeIndex = 0;
      for (TInt j=0; j < Group.Len(); j++) {
        if (Mapping[Group[j]] != Invalid) {
          Group[FreeIndex] = Mapping[Group[j]];
          FreeIndex++;
        }
      }
      // resize to get rid of end values
      Group.Trunc(FreeIndex);
    }
  }
  // should match, or bug somewhere
  Assert (NumValidRows == NumRows);
}

// wrong reading of string attributes
void TTable::BuildGraphTopology(PNEANet& Graph, THash<TFlt, TInt>& FSrNodeMap, THash<TFlt, TInt>& FDsNodeMap) {
  TYPE SrCT = GetColType(SrcCol);
  TInt SrIdx = GetColIdx(SrcCol);
  TYPE DsCT = GetColType(DstCol);
  TInt DsIdx = GetColIdx(DstCol);
  TInt SrcCnt = 0;
  TInt DstCnt = 0;
  
  for(TRowIterator RowI = BegRI(); RowI < EndRI(); RowI++) {
    if (SrCT == INT && DsCT == INT) {
      Graph->AddNode(IntCols[SrIdx][RowI.GetRowIdx()]);
      Graph->AddNode(IntCols[DsIdx][RowI.GetRowIdx()]);
      Graph->AddEdge(IntCols[SrIdx][RowI.GetRowIdx()], IntCols[DsIdx][RowI.GetRowIdx()], RowI.GetRowIdx());
    } else if (SrCT == INT && DsCT == FLT) {
      Graph->AddNode(IntCols[SrIdx][RowI.GetRowIdx()]);
      TFlt val = FltCols[DsIdx][RowI.GetRowIdx()];
      if (!FDsNodeMap.IsKey(val)) {
	      FDsNodeMap.AddDat(val, DstCnt++);
      }
      Graph->AddNode(FDsNodeMap.GetDat(val));
      Graph->AddEdge(IntCols[SrIdx][RowI.GetRowIdx()], FDsNodeMap.GetDat(val));
    } else if (SrCT == INT && DsCT == STR) {
      Graph->AddNode(IntCols[SrIdx][RowI.GetRowIdx()]);
      Graph->AddNode(StrColMaps[DsIdx][RowI.GetRowIdx()]);
      Graph->AddEdge(IntCols[SrIdx][RowI.GetRowIdx()], StrColMaps[DsIdx][RowI.GetRowIdx()], RowI.GetRowIdx());
    } else if (SrCT == FLT && DsCT == INT) {
      Graph->AddNode(IntCols[DsIdx][RowI.GetRowIdx()]);
      TFlt val = FltCols[SrIdx][RowI.GetRowIdx()];
      if (!FSrNodeMap.IsKey(val)) {
	      FSrNodeMap.AddDat(val, SrcCnt++);
      }
      Graph->AddNode(FSrNodeMap.GetDat(val));
      Graph->AddEdge(FSrNodeMap.GetDat(val), IntCols[SrIdx][RowI.GetRowIdx()], RowI.GetRowIdx());
    } else if (SrCT == FLT && DsCT == STR) {
      Graph->AddNode(StrColMaps[DsIdx][RowI.GetRowIdx()]);
      TFlt val = FltCols[SrIdx][RowI.GetRowIdx()];
      if (!FSrNodeMap.IsKey(val)) {
	      FSrNodeMap.AddDat(val, SrcCnt++);
      }
      Graph->AddNode(FSrNodeMap.GetDat(val));
      Graph->AddEdge(FSrNodeMap.GetDat(val), IntCols[SrIdx][RowI.GetRowIdx()], RowI.GetRowIdx());
    } else if (SrCT == FLT && DsCT == FLT) {
      TFlt val = FltCols[SrIdx][RowI.GetRowIdx()];
      if (!FSrNodeMap.IsKey(val)) {
	      FSrNodeMap.AddDat(val, SrcCnt++);
      }
      Graph->AddNode(FSrNodeMap.GetDat(val));
      val = FltCols[DsIdx][RowI.GetRowIdx()];
      if (!FDsNodeMap.IsKey(val)) {
	      FDsNodeMap.AddDat(val, DstCnt++);
      }
      Graph->AddNode(FDsNodeMap.GetDat(val));
      Graph->AddEdge(FSrNodeMap.GetDat(val), FDsNodeMap.GetDat(val), RowI.GetRowIdx());
    }
  }
}

TInt TTable::GetNId(TStr Col, TInt RowIdx, THash<TFlt, TInt>& FSrNodeMap, THash<TFlt, TInt>& FDsNodeMap) {
  TYPE CT = GetColType(Col);
  TInt Idx = GetColIdx(Col);
  if (CT == INT) {
    return IntCols[RowIdx][Idx];
  } else if (CT == FLT) {
    if (Col == SrcCol) {
      return FSrNodeMap.GetDat(FltCols[Idx][RowIdx]);
    } else if (Col == DstCol) {
      return FDsNodeMap.GetDat(FltCols[Idx][RowIdx]);
    } else {
      TExcept::Throw("Column " + Col + " is not source node or destination column");
    }
  } else {
    return StrColMaps[RowIdx][Idx]();
  }
  return 0;
}
    
void TTable::AddNodeAttributes(PNEANet& Graph, THash<TFlt, TInt>& FSrNodeMap, THash<TFlt, TInt>& FDsNodeMap) {
  for (TRowIterator RowI = BegRI(); RowI < EndRI(); RowI++) {
    // Add Source and Destination node attributes.
    for (int i = 0; i < SrcNodeAttrV.Len(); i++) {
      TStr SrcColAttr = SrcNodeAttrV[i];
      TYPE CT = GetColType(SrcColAttr);
      int Idx = GetColIdx(SrcColAttr);
      TInt RowIdx  = RowI.GetRowIdx();
      if (CT == INT) {
	      Graph->AddIntAttrDatN(GetNId(SrcCol, RowIdx, FSrNodeMap, FDsNodeMap), IntCols[Idx][RowIdx], SrcColAttr);
      } else if (CT == FLT) {
	      Graph->AddFltAttrDatN(GetNId(SrcCol, RowIdx, FSrNodeMap, FDsNodeMap), FltCols[Idx][RowIdx], SrcColAttr);
      } else {
	      Graph->AddStrAttrDatN(GetNId(SrcCol, RowIdx, FSrNodeMap, FDsNodeMap), StrColVals.GetStr(StrColMaps[Idx][RowIdx]), SrcColAttr);
      }
    }

    for (int i = 0; i < DstNodeAttrV.Len(); i++) {
      TStr DstColAttr = DstNodeAttrV[i];
      TYPE CT = GetColType(DstColAttr);
      int Idx = GetColIdx(DstColAttr);
      TInt RowIdx  = RowI.GetRowIdx();
      if (CT == INT) {
	      Graph->AddIntAttrDatN(GetNId(SrcCol, RowIdx, FSrNodeMap, FDsNodeMap), IntCols[Idx][RowIdx], DstColAttr);
      } else if (CT == FLT) {
	      Graph->AddFltAttrDatN(GetNId(SrcCol, RowIdx, FSrNodeMap, FDsNodeMap), FltCols[Idx][RowIdx], DstColAttr);
      } else {
	      Graph->AddStrAttrDatN(GetNId(SrcCol, RowIdx, FSrNodeMap, FDsNodeMap), StrColVals.GetStr(StrColMaps[Idx][RowIdx]), DstColAttr);
      }
    }
  }
}

void TTable::AddEdgeAttributes(PNEANet& Graph) {
  for(TRowIterator RowI = BegRI(); RowI < EndRI(); RowI++) {
    typedef THash<TStr,TPair<TYPE,TInt> >::TIter TColIter;
    for (int i = 0; i < EdgeAttrV.Len(); i++) {
      TStr ColName = EdgeAttrV[i];
      TYPE T = GetColType(ColName);
      TInt Index = GetColIdx(ColName);
      TInt Ival;
      TFlt Fval;
      TStr Sval;
      switch (T) {
      case INT:
	Ival = IntCols[Index][RowI.GetRowIdx()];
	Graph->AddIntAttrDatE(RowI.GetRowIdx(), Ival, ColName);
	break;
      case FLT:
	Fval = FltCols[Index][RowI.GetRowIdx()];
	Graph->AddFltAttrDatE(RowI.GetRowIdx(), Fval, ColName);
	break;
      case STR:
	Sval = StrColVals.GetStr(StrColMaps[Index][RowI.GetRowIdx()]);
	Graph->AddStrAttrDatE(RowI.GetRowIdx(), Sval, ColName);
	break;
      }
    }
  }
}

PNEANet TTable::ToGraph() {
  PNEANet Graph = PNEANet::New();
  THash<TFlt, TInt> FSrNodeMap = THash<TFlt, TInt>();
  THash<TFlt, TInt> FDsNodeMap = THash<TFlt, TInt>();
  BuildGraphTopology(Graph, FSrNodeMap, FDsNodeMap);
  AddEdgeAttributes(Graph);
  AddNodeAttributes(Graph, FSrNodeMap, FDsNodeMap);
  return Graph;
}




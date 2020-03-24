; ModuleID = 'src/hpvm/../../common_src/convert-dataset/convert_dataset.c'
source_filename = "src/hpvm/../../common_src/convert-dataset/convert_dataset.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, i8*, i8*, i8*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type { %struct._IO_marker*, %struct._IO_FILE*, i32 }
%struct._mat_entry = type { i32, i32, float }
%struct._row_stats = type { i32, i32, i32, i32 }

@.str = private unnamed_addr constant [2 x i8] c"r\00", align 1
@.str.2 = private unnamed_addr constant [42 x i8] c"Sorry, this application does not support \00", align 1
@.str.3 = private unnamed_addr constant [26 x i8] c"Market Market type: [%s]\0A\00", align 1
@.str.4 = private unnamed_addr constant [10 x i8] c"%d %d %f\0A\00", align 1
@.str.5 = private unnamed_addr constant [7 x i8] c"%d %d\0A\00", align 1
@.str.6 = private unnamed_addr constant [113 x i8] c"Converting COO to JDS format (%dx%d)\0A%d matrix entries, warp size = %d, row padding align = %d, pack size = %d\0A\0A\00", align 1
@stdin = external dso_local local_unnamed_addr global %struct._IO_FILE*, align 8
@.str.7 = private unnamed_addr constant [36 x i8] c"Padding data....%d rows, %d groups\0A\00", align 1
@.str.8 = private unnamed_addr constant [44 x i8] c"Padding warp group %d to %d items, zn = %d\0A\00", align 1
@.str.9 = private unnamed_addr constant [50 x i8] c"Allocating data space: %d entries (%f%% padding)\0A\00", align 1
@.str.10 = private unnamed_addr constant [16 x i8] c"[%d row%d=%.3f]\00", align 1
@.str.11 = private unnamed_addr constant [3 x i8] c"%d\00", align 1
@.str.14 = private unnamed_addr constant [58 x i8] c"Finished converting.\0AJDS format has %d columns, %d rows.\0A\00", align 1
@.str.15 = private unnamed_addr constant [19 x i8] c"nz_count_len = %d\0A\00", align 1
@str = private unnamed_addr constant [40 x i8] c"Could not process Matrix Market banner.\00", align 1

; Function Attrs: norecurse nounwind readonly uwtable
define dso_local i32 @sort_rows(i8* nocapture readonly %a, i8* nocapture readonly %b) #0 {
entry:
  %row = bitcast i8* %a to i32*
  %0 = load i32, i32* %row, align 4, !tbaa !2
  %row1 = bitcast i8* %b to i32*
  %1 = load i32, i32* %row1, align 4, !tbaa !2
  %sub = sub nsw i32 %0, %1
  ret i32 %sub
}

; Function Attrs: norecurse nounwind readonly uwtable
define dso_local i32 @sort_cols(i8* nocapture readonly %a, i8* nocapture readonly %b) local_unnamed_addr #0 {
entry:
  %col = getelementptr inbounds i8, i8* %a, i64 4
  %0 = bitcast i8* %col to i32*
  %1 = load i32, i32* %0, align 4, !tbaa !8
  %col1 = getelementptr inbounds i8, i8* %b, i64 4
  %2 = bitcast i8* %col1 to i32*
  %3 = load i32, i32* %2, align 4, !tbaa !8
  %sub = sub nsw i32 %1, %3
  ret i32 %sub
}

; Function Attrs: norecurse nounwind readonly uwtable
define dso_local i32 @sort_stats(i8* nocapture readonly %a, i8* nocapture readonly %b) #0 {
entry:
  %size = getelementptr inbounds i8, i8* %b, i64 4
  %0 = bitcast i8* %size to i32*
  %1 = load i32, i32* %0, align 4, !tbaa !9
  %size1 = getelementptr inbounds i8, i8* %a, i64 4
  %2 = bitcast i8* %size1 to i32*
  %3 = load i32, i32* %2, align 4, !tbaa !9
  %sub = sub nsw i32 %1, %3
  ret i32 %sub
}

; Function Attrs: nounwind uwtable
define dso_local i32 @coo_to_jds(i8* nocapture readonly %mtx_filename, i32 %pad_rows, i32 %warp_size, i32 %pack_size, i32 %mirrored, i32 %binary, i32 %debug_level, float** nocapture %data, i32** nocapture %data_row_ptr, i32** nocapture %nz_count, i32** nocapture %data_col_index, i32** nocapture %data_row_map, i32* nocapture %data_cols, i32* nocapture %dim, i32* nocapture %len, i32* nocapture %nz_count_len, i32* nocapture %data_ptr_len) local_unnamed_addr #1 {
entry:
  %matcode = alloca [4 x i8], align 1
  %nz = alloca i32, align 4
  %rows = alloca i32, align 4
  %cols = alloca i32, align 4
  %0 = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #10
  %1 = bitcast i32* %nz to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %1) #10
  %2 = bitcast i32* %rows to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %2) #10
  %3 = bitcast i32* %cols to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %3) #10
  %call = tail call %struct._IO_FILE* @fopen(i8* %mtx_filename, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str, i64 0, i64 0))
  %cmp = icmp eq %struct._IO_FILE* %call, null
  br i1 %cmp, label %if.then, label %if.end

if.then:                                          ; preds = %entry
  tail call void @exit(i32 1) #11
  unreachable

if.end:                                           ; preds = %entry
  %call1 = call i32 @mm_read_banner(%struct._IO_FILE* nonnull %call, [4 x i8]* nonnull %matcode) #10
  %cmp2 = icmp eq i32 %call1, 0
  br i1 %cmp2, label %if.end5, label %if.then3

if.then3:                                         ; preds = %if.end
  %puts = call i32 @puts(i8* getelementptr inbounds ([40 x i8], [40 x i8]* @str, i64 0, i64 0))
  call void @exit(i32 1) #11
  unreachable

if.end5:                                          ; preds = %if.end
  %arrayidx = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 2
  %4 = load i8, i8* %arrayidx, align 1, !tbaa !11
  %cmp6 = icmp eq i8 %4, 67
  %5 = load i8, i8* %0, align 1
  %cmp10 = icmp eq i8 %5, 77
  %or.cond576 = and i1 %cmp6, %cmp10
  br i1 %or.cond576, label %land.lhs.true12, label %if.end21

land.lhs.true12:                                  ; preds = %if.end5
  %arrayidx13 = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 1
  %6 = load i8, i8* %arrayidx13, align 1, !tbaa !11
  %cmp15 = icmp eq i8 %6, 67
  br i1 %cmp15, label %if.then17, label %if.end21

if.then17:                                        ; preds = %land.lhs.true12
  %call18 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([42 x i8], [42 x i8]* @.str.2, i64 0, i64 0))
  %call19 = call i8* @mm_typecode_to_str(i8* nonnull %0) #10
  %call20 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([26 x i8], [26 x i8]* @.str.3, i64 0, i64 0), i8* %call19)
  call void @exit(i32 1) #11
  unreachable

if.end21:                                         ; preds = %land.lhs.true12, %if.end5
  %call22 = call i32 @mm_read_mtx_crd_size(%struct._IO_FILE* nonnull %call, i32* nonnull %rows, i32* nonnull %cols, i32* nonnull %nz) #10
  %cmp23 = icmp eq i32 %call22, 0
  br i1 %cmp23, label %if.end26, label %if.then25

if.then25:                                        ; preds = %if.end21
  call void @exit(i32 1) #11
  unreachable

if.end26:                                         ; preds = %if.end21
  %7 = load i32, i32* %rows, align 4, !tbaa !12
  store i32 %7, i32* %dim, align 4, !tbaa !12
  %tobool = icmp ne i32 %mirrored, 0
  %8 = load i32, i32* %nz, align 4, !tbaa !12
  br i1 %tobool, label %if.then27, label %if.else

if.then27:                                        ; preds = %if.end26
  %mul = shl nsw i32 %8, 1
  %conv28 = sext i32 %mul to i64
  %mul29 = mul nsw i64 %conv28, 12
  %call30 = call noalias i8* @malloc(i64 %mul29) #10
  %.pr = load i32, i32* %nz, align 4, !tbaa !12
  br label %if.end34

if.else:                                          ; preds = %if.end26
  %conv31 = sext i32 %8 to i64
  %mul32 = mul nsw i64 %conv31, 12
  %call33 = call noalias i8* @malloc(i64 %mul32) #10
  br label %if.end34

if.end34:                                         ; preds = %if.else, %if.then27
  %9 = phi i32 [ %8, %if.else ], [ %.pr, %if.then27 ]
  %entries.0.in = phi i8* [ %call33, %if.else ], [ %call30, %if.then27 ]
  %entries.0 = bitcast i8* %entries.0.in to %struct._mat_entry*
  %cmp35611 = icmp sgt i32 %9, 0
  br i1 %cmp35611, label %for.body.lr.ph, label %for.end

for.body.lr.ph:                                   ; preds = %if.end34
  %tobool37 = icmp eq i32 %binary, 0
  %tobool.not = xor i1 %tobool, true
  br label %for.body

for.body:                                         ; preds = %for.body.lr.ph, %for.inc
  %i.0613 = phi i32 [ 0, %for.body.lr.ph ], [ %inc98, %for.inc ]
  %cur_i.0612 = phi i32 [ 0, %for.body.lr.ph ], [ %inc99, %for.inc ]
  %idxprom47 = sext i32 %cur_i.0612 to i64
  %row49 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %idxprom47, i32 0
  %col52 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %idxprom47, i32 1
  br i1 %tobool37, label %if.then38, label %if.else46

if.then38:                                        ; preds = %for.body
  %val44 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %idxprom47, i32 2
  %call45 = call i32 (%struct._IO_FILE*, i8*, ...) @__isoc99_fscanf(%struct._IO_FILE* nonnull %call, i8* getelementptr inbounds ([10 x i8], [10 x i8]* @.str.4, i64 0, i64 0), i32* %row49, i32* nonnull %col52, float* nonnull %val44) #10
  br label %if.end57

if.else46:                                        ; preds = %for.body
  %call53 = call i32 (%struct._IO_FILE*, i8*, ...) @__isoc99_fscanf(%struct._IO_FILE* nonnull %call, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.5, i64 0, i64 0), i32* %row49, i32* nonnull %col52) #10
  %val56 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %idxprom47, i32 2
  store float 1.000000e+00, float* %val56, align 4, !tbaa !13
  br label %if.end57

if.end57:                                         ; preds = %if.else46, %if.then38
  %10 = load i32, i32* %row49, align 4, !tbaa !2
  %dec = add nsw i32 %10, -1
  store i32 %dec, i32* %row49, align 4, !tbaa !2
  %11 = load i32, i32* %col52, align 4, !tbaa !8
  %dec64 = add nsw i32 %11, -1
  store i32 %dec64, i32* %col52, align 4, !tbaa !8
  %cmp73 = icmp eq i32 %dec, %dec64
  %or.cond577 = or i1 %cmp73, %tobool.not
  br i1 %or.cond577, label %for.inc, label %if.then75

if.then75:                                        ; preds = %if.end57
  %inc = add nsw i32 %cur_i.0612, 1
  %val78 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %idxprom47, i32 2
  %12 = bitcast float* %val78 to i32*
  %13 = load i32, i32* %12, align 4, !tbaa !13
  %idxprom79 = sext i32 %inc to i64
  %val81 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %idxprom79, i32 2
  %14 = bitcast float* %val81 to i32*
  store i32 %13, i32* %14, align 4, !tbaa !13
  %col88 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %idxprom79, i32 1
  store i32 %dec, i32* %col88, align 4, !tbaa !8
  %row95 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %idxprom79, i32 0
  store i32 %dec64, i32* %row95, align 4, !tbaa !2
  br label %for.inc

for.inc:                                          ; preds = %if.end57, %if.then75
  %cur_i.1 = phi i32 [ %inc, %if.then75 ], [ %cur_i.0612, %if.end57 ]
  %inc98 = add nuw nsw i32 %i.0613, 1
  %inc99 = add nsw i32 %cur_i.1, 1
  %15 = load i32, i32* %nz, align 4, !tbaa !12
  %cmp35 = icmp slt i32 %inc98, %15
  br i1 %cmp35, label %for.body, label %for.end

for.end:                                          ; preds = %for.inc, %if.end34
  %cur_i.0.lcssa = phi i32 [ 0, %if.end34 ], [ %inc99, %for.inc ]
  store i32 %cur_i.0.lcssa, i32* %nz, align 4, !tbaa !12
  %cmp100 = icmp sgt i32 %debug_level, 0
  br i1 %cmp100, label %if.then102, label %if.end104

if.then102:                                       ; preds = %for.end
  %16 = load i32, i32* %rows, align 4, !tbaa !12
  %17 = load i32, i32* %cols, align 4, !tbaa !12
  %call103 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([113 x i8], [113 x i8]* @.str.6, i64 0, i64 0), i32 %16, i32 %17, i32 %cur_i.0.lcssa, i32 %warp_size, i32 %pad_rows, i32 %pack_size)
  br label %if.end104

if.end104:                                        ; preds = %if.then102, %for.end
  %18 = load %struct._IO_FILE*, %struct._IO_FILE** @stdin, align 8, !tbaa !14
  %cmp105 = icmp eq %struct._IO_FILE* %call, %18
  br i1 %cmp105, label %if.end109, label %if.then107

if.then107:                                       ; preds = %if.end104
  %call108 = call i32 @fclose(%struct._IO_FILE* nonnull %call)
  br label %if.end109

if.end109:                                        ; preds = %if.end104, %if.then107
  %19 = load i32, i32* %nz, align 4, !tbaa !12
  %conv110 = sext i32 %19 to i64
  call void @qsort(i8* %entries.0.in, i64 %conv110, i64 12, i32 (i8*, i8*)* nonnull @sort_rows) #10
  %20 = load i32, i32* %nz, align 4, !tbaa !12
  %sub111 = add nsw i32 %20, -1
  %idxprom112 = sext i32 %sub111 to i64
  %row114 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %idxprom112, i32 0
  %21 = load i32, i32* %row114, align 4, !tbaa !2
  %add = add nsw i32 %21, 1
  %rem = srem i32 %add, %warp_size
  %tobool115 = icmp eq i32 %rem, 0
  %sub118 = sub nsw i32 %warp_size, %rem
  %add119 = select i1 %tobool115, i32 0, i32 %sub118
  %storemerge = add nsw i32 %add119, %add
  store i32 %storemerge, i32* %rows, align 4, !tbaa !12
  %conv121 = sext i32 %storemerge to i64
  %call122 = call noalias i8* @calloc(i64 %conv121, i64 16) #10
  %22 = bitcast i8* %call122 to %struct._row_stats*
  %call124 = call noalias i8* @calloc(i64 %conv121, i64 4) #10
  %23 = bitcast i32** %data_row_map to i8**
  store i8* %call124, i8** %23, align 8, !tbaa !14
  %cmp128605 = icmp sgt i32 %20, 0
  br i1 %cmp128605, label %for.body130.preheader, label %for.end163

for.body130.preheader:                            ; preds = %if.end109
  %row126 = bitcast i8* %entries.0.in to i32*
  %24 = load i32, i32* %row126, align 4, !tbaa !2
  %25 = zext i32 %sub111 to i64
  %wide.trip.count = zext i32 %20 to i64
  br label %for.body130

for.body130:                                      ; preds = %if.end159, %for.body130.preheader
  %indvars.iv623 = phi i64 [ 0, %for.body130.preheader ], [ %indvars.iv.next624, %if.end159 ]
  %irow.0608 = phi i32 [ %24, %for.body130.preheader ], [ %irow.1, %if.end159 ]
  %istart.0607 = phi i32 [ 0, %for.body130.preheader ], [ %istart.1, %if.end159 ]
  %icol.0606 = phi i32 [ 0, %for.body130.preheader ], [ %inc160, %if.end159 ]
  %row133 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %indvars.iv623, i32 0
  %26 = load i32, i32* %row133, align 4, !tbaa !2
  %cmp134 = icmp ne i32 %26, %irow.0608
  %cmp137 = icmp eq i64 %indvars.iv623, %25
  %or.cond = or i1 %cmp137, %cmp134
  br i1 %or.cond, label %if.then139, label %if.end159

if.then139:                                       ; preds = %for.body130
  %inc144 = zext i1 %cmp137 to i32
  %spec.select = add nsw i32 %icol.0606, %inc144
  %idxprom146 = sext i32 %irow.0608 to i64
  %size = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %idxprom146, i32 1
  store i32 %spec.select, i32* %size, align 4, !tbaa !9
  %27 = add nsw i64 %indvars.iv623, -1
  %row151 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %27, i32 0
  %28 = load i32, i32* %row151, align 4, !tbaa !2
  %index = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %idxprom146, i32 0
  store i32 %28, i32* %index, align 4, !tbaa !16
  %start = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %idxprom146, i32 2
  store i32 %istart.0607, i32* %start, align 4, !tbaa !17
  %29 = trunc i64 %indvars.iv623 to i32
  br label %if.end159

if.end159:                                        ; preds = %for.body130, %if.then139
  %icol.2 = phi i32 [ 0, %if.then139 ], [ %icol.0606, %for.body130 ]
  %istart.1 = phi i32 [ %29, %if.then139 ], [ %istart.0607, %for.body130 ]
  %irow.1 = phi i32 [ %26, %if.then139 ], [ %irow.0608, %for.body130 ]
  %inc160 = add nsw i32 %icol.2, 1
  %indvars.iv.next624 = add nuw nsw i64 %indvars.iv623, 1
  %exitcond626 = icmp eq i64 %indvars.iv.next624, %wide.trip.count
  br i1 %exitcond626, label %for.end163, label %for.body130

for.end163:                                       ; preds = %if.end159, %if.end109
  %div = sdiv i32 %storemerge, %warp_size
  %rem164 = srem i32 %storemerge, %warp_size
  %add165 = add nsw i32 %rem164, %div
  store i32 %add165, i32* %nz_count_len, align 4, !tbaa !12
  %conv166 = sext i32 %add165 to i64
  %mul167 = shl nsw i64 %conv166, 2
  %call168 = call noalias i8* @malloc(i64 %mul167) #10
  %30 = bitcast i32** %nz_count to i8**
  store i8* %call168, i8** %30, align 8, !tbaa !14
  call void @qsort(i8* %call122, i64 %conv121, i64 16, i32 (i8*, i8*)* nonnull @sort_stats) #10
  br i1 %cmp100, label %if.then172, label %if.end174

if.then172:                                       ; preds = %for.end163
  %31 = load i32, i32* %rows, align 4, !tbaa !12
  %32 = load i32, i32* %nz_count_len, align 4, !tbaa !12
  %call173 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([36 x i8], [36 x i8]* @.str.7, i64 0, i64 0), i32 %31, i32 %32)
  br label %if.end174

if.end174:                                        ; preds = %if.then172, %for.end163
  %mul175 = mul nsw i32 %pack_size, %pad_rows
  %33 = load i32, i32* %rows, align 4, !tbaa !12
  %cmp177597 = icmp sgt i32 %33, 0
  br i1 %cmp177597, label %for.body179.lr.ph, label %for.end265

for.body179.lr.ph:                                ; preds = %if.end174
  %conv222 = sitofp i32 %pack_size to float
  %cmp244 = icmp sgt i32 %debug_level, 1
  br label %for.body179

for.body179:                                      ; preds = %for.body179.lr.ph, %if.end258
  %indvars.iv621 = phi i64 [ 0, %for.body179.lr.ph ], [ %indvars.iv.next622, %if.end258 ]
  %total_padding.0600 = phi i32 [ 0, %for.body179.lr.ph ], [ %add262, %if.end258 ]
  %pad_to.0599 = phi i32 [ undef, %for.body179.lr.ph ], [ %pad_to.1, %if.end258 ]
  %total_size.0598 = phi i32 [ 0, %for.body179.lr.ph ], [ %total_size.1, %if.end258 ]
  %index182 = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %indvars.iv621, i32 0
  %34 = load i32, i32* %index182, align 4, !tbaa !16
  %35 = load i32*, i32** %data_row_map, align 8, !tbaa !14
  %arrayidx184 = getelementptr inbounds i32, i32* %35, i64 %indvars.iv621
  store i32 %34, i32* %arrayidx184, align 4, !tbaa !12
  %36 = trunc i64 %indvars.iv621 to i32
  %rem190 = srem i32 %36, %warp_size
  %cmp191 = icmp eq i32 %rem190, 0
  %size196 = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %indvars.iv621, i32 1
  %37 = load i32, i32* %size196, align 4, !tbaa !9
  br i1 %cmp191, label %if.then193, label %if.else250

if.then193:                                       ; preds = %for.body179
  %rem197 = srem i32 %37, %mul175
  %tobool198 = icmp eq i32 %rem197, 0
  %sub204 = sub nsw i32 %mul175, %rem197
  %.sink = select i1 %tobool198, i32 0, i32 %sub204
  %padding210 = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %indvars.iv621, i32 3
  store i32 %.sink, i32* %padding210, align 4, !tbaa !18
  %rem215 = srem i32 %37, %pack_size
  %div230 = sdiv i32 %37, %pack_size
  %tobool216 = icmp eq i32 %rem215, 0
  br i1 %tobool216, label %if.end231, label %if.then217

if.then217:                                       ; preds = %if.then193
  %conv221 = sitofp i32 %37 to float
  %38 = fdiv fast float %conv221, %conv222
  %39 = call fast float @llvm.ceil.f32(float %38)
  %conv225 = fptosi float %39 to i32
  br label %if.end231

if.end231:                                        ; preds = %if.then193, %if.then217
  %pack_to.0 = phi i32 [ %conv225, %if.then217 ], [ %div230, %if.then193 ]
  %padding237 = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %indvars.iv621, i32 3
  %40 = load i32, i32* %padding237, align 4, !tbaa !18
  %add238 = add nsw i32 %40, %37
  %41 = load i32*, i32** %nz_count, align 8, !tbaa !14
  %42 = trunc i64 %indvars.iv621 to i32
  %div239 = sdiv i32 %42, %warp_size
  %idxprom240 = sext i32 %div239 to i64
  %arrayidx241 = getelementptr inbounds i32, i32* %41, i64 %idxprom240
  store i32 %pack_to.0, i32* %arrayidx241, align 4, !tbaa !12
  %mul242 = mul nsw i32 %add238, %warp_size
  %add243 = add nsw i32 %mul242, %total_size.0598
  br i1 %cmp244, label %if.then246, label %if.end258

if.then246:                                       ; preds = %if.end231
  %call248 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([44 x i8], [44 x i8]* @.str.8, i64 0, i64 0), i32 %div239, i32 %add238, i32 %pack_to.0)
  br label %if.end258

if.else250:                                       ; preds = %for.body179
  %sub254 = sub nsw i32 %pad_to.0599, %37
  %padding257 = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %indvars.iv621, i32 3
  store i32 %sub254, i32* %padding257, align 4, !tbaa !18
  br label %if.end258

if.end258:                                        ; preds = %if.end231, %if.then246, %if.else250
  %total_size.1 = phi i32 [ %add243, %if.then246 ], [ %add243, %if.end231 ], [ %total_size.0598, %if.else250 ]
  %pad_to.1 = phi i32 [ %add238, %if.then246 ], [ %add238, %if.end231 ], [ %pad_to.0599, %if.else250 ]
  %padding261 = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %indvars.iv621, i32 3
  %43 = load i32, i32* %padding261, align 4, !tbaa !18
  %add262 = add nsw i32 %43, %total_padding.0600
  %indvars.iv.next622 = add nuw nsw i64 %indvars.iv621, 1
  %44 = load i32, i32* %rows, align 4, !tbaa !12
  %45 = sext i32 %44 to i64
  %cmp177 = icmp slt i64 %indvars.iv.next622, %45
  br i1 %cmp177, label %for.body179, label %for.end265.loopexit

for.end265.loopexit:                              ; preds = %if.end258
  %phitmp = sitofp i32 %add262 to float
  %phitmp627 = fmul fast float %phitmp, 1.000000e+02
  br label %for.end265

for.end265:                                       ; preds = %for.end265.loopexit, %if.end174
  %total_size.0.lcssa = phi i32 [ 0, %if.end174 ], [ %total_size.1, %for.end265.loopexit ]
  %total_padding.0.lcssa = phi float [ 0.000000e+00, %if.end174 ], [ %phitmp627, %for.end265.loopexit ]
  br i1 %cmp100, label %if.then268, label %if.end275

if.then268:                                       ; preds = %for.end265
  %conv271 = sitofp i32 %total_size.0.lcssa to float
  %div272 = fdiv fast float %total_padding.0.lcssa, %conv271
  %conv273 = fpext float %div272 to double
  %call274 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([50 x i8], [50 x i8]* @.str.9, i64 0, i64 0), i32 %total_size.0.lcssa, double %conv273)
  br label %if.end275

if.end275:                                        ; preds = %if.then268, %for.end265
  %conv276 = sext i32 %total_size.0.lcssa to i64
  %call277 = call noalias i8* @calloc(i64 %conv276, i64 4) #10
  %46 = bitcast float** %data to i8**
  store i8* %call277, i8** %46, align 8, !tbaa !14
  %call279 = call noalias i8* @calloc(i64 %conv276, i64 4) #10
  %47 = bitcast i32** %data_col_index to i8**
  store i8* %call279, i8** %47, align 8, !tbaa !14
  %48 = load i32, i32* %rows, align 4, !tbaa !12
  %conv280 = sext i32 %48 to i64
  %call281 = call noalias i8* @calloc(i64 %conv280, i64 4) #10
  %49 = bitcast i32** %data_row_ptr to i8**
  store i8* %call281, i8** %49, align 8, !tbaa !14
  store i32 %total_size.0.lcssa, i32* %len, align 4, !tbaa !12
  %50 = load i32*, i32** %data_row_ptr, align 8, !tbaa !14
  store i32 0, i32* %50, align 4, !tbaa !12
  %size286 = getelementptr inbounds i8, i8* %call122, i64 4
  %51 = bitcast i8* %size286 to i32*
  %52 = load i32, i32* %51, align 4, !tbaa !9
  %padding288 = getelementptr inbounds i8, i8* %call122, i64 12
  %53 = bitcast i8* %padding288 to i32*
  %54 = load i32, i32* %53, align 4, !tbaa !18
  %add289591 = add nsw i32 %54, %52
  %cmp291592 = icmp sgt i32 %add289591, 0
  br i1 %cmp291592, label %for.cond295.preheader.lr.ph, label %while.end

for.cond295.preheader.lr.ph:                      ; preds = %if.end275
  %cmp300581 = icmp sgt i32 %pack_size, 0
  %cmp353 = icmp sgt i32 %debug_level, 1
  %cmp325 = icmp sgt i32 %debug_level, 1
  %cmp370 = icmp sgt i32 %debug_level, 1
  br label %for.cond295.preheader

for.cond295.preheader:                            ; preds = %for.cond295.preheader.lr.ph, %if.end374
  %indvars.iv619 = phi i64 [ 0, %for.cond295.preheader.lr.ph ], [ %indvars.iv.next620, %if.end374 ]
  %mul290595 = phi i32 [ 0, %for.cond295.preheader.lr.ph ], [ %mul290, %if.end374 ]
  %idata.0593 = phi i32 [ 0, %for.cond295.preheader.lr.ph ], [ %idata.3, %if.end374 ]
  %55 = load i32, i32* %rows, align 4, !tbaa !12
  %cmp296586 = icmp sgt i32 %55, 0
  br i1 %cmp296586, label %for.cond299.preheader, label %endwrite

for.cond299.preheader:                            ; preds = %for.cond295.preheader, %for.inc367
  %indvars.iv617 = phi i64 [ %indvars.iv.next618, %for.inc367 ], [ 0, %for.cond295.preheader ]
  %idata.1587 = phi i32 [ %idata.2.lcssa, %for.inc367 ], [ %idata.0593, %for.cond295.preheader ]
  br i1 %cmp300581, label %for.body302.lr.ph, label %for.inc367

for.body302.lr.ph:                                ; preds = %for.cond299.preheader
  %size305 = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %indvars.iv617, i32 1
  %padding346 = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %indvars.iv617, i32 3
  %start313 = getelementptr inbounds %struct._row_stats, %struct._row_stats* %22, i64 %indvars.iv617, i32 2
  %cmp328 = icmp ult i64 %indvars.iv617, 3
  %56 = sext i32 %idata.1587 to i64
  %57 = trunc i64 %indvars.iv617 to i32
  br label %for.body302

for.body302:                                      ; preds = %if.end362, %for.body302.lr.ph
  %indvars.iv = phi i64 [ %56, %for.body302.lr.ph ], [ %indvars.iv.next, %if.end362 ]
  %ipack.0584 = phi i32 [ 0, %for.body302.lr.ph ], [ %inc365, %if.end362 ]
  %58 = load i32, i32* %size305, align 4, !tbaa !9
  %add307 = add nsw i32 %ipack.0584, %mul290595
  %cmp308 = icmp sgt i32 %58, %add307
  br i1 %cmp308, label %if.then310, label %if.else340

if.then310:                                       ; preds = %for.body302
  %59 = load i32, i32* %start313, align 4, !tbaa !17
  %add315 = add i32 %ipack.0584, %mul290595
  %add316 = add i32 %add315, %59
  %idxprom317 = sext i32 %add316 to i64
  %entry282.sroa.3.0..sroa_idx384 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %idxprom317, i32 1
  %entry282.sroa.3.0.copyload = load i32, i32* %entry282.sroa.3.0..sroa_idx384, align 4
  %entry282.sroa.4.0..sroa_idx386 = getelementptr inbounds %struct._mat_entry, %struct._mat_entry* %entries.0, i64 %idxprom317, i32 2
  %entry282.sroa.4.0.copyload = load float, float* %entry282.sroa.4.0..sroa_idx386, align 4
  %60 = load float*, float** %data, align 8, !tbaa !14
  %arrayidx321 = getelementptr inbounds float, float* %60, i64 %indvars.iv
  store float %entry282.sroa.4.0.copyload, float* %arrayidx321, align 4, !tbaa !19
  %61 = load i32*, i32** %data_col_index, align 8, !tbaa !14
  %arrayidx324 = getelementptr inbounds i32, i32* %61, i64 %indvars.iv
  store i32 %entry282.sroa.3.0.copyload, i32* %arrayidx324, align 4, !tbaa !12
  br i1 %cmp325, label %if.then327, label %if.end362

if.then327:                                       ; preds = %if.then310
  %add331 = add nuw nsw i32 %ipack.0584, 1
  br i1 %cmp328, label %if.then330, label %if.else335

if.then330:                                       ; preds = %if.then327
  %conv333 = fpext float %entry282.sroa.4.0.copyload to double
  %call334 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([16 x i8], [16 x i8]* @.str.10, i64 0, i64 0), i32 %add331, i32 %57, double %conv333)
  br label %if.end362

if.else335:                                       ; preds = %if.then327
  %call337 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str.11, i64 0, i64 0), i32 %add331)
  br label %if.end362

if.else340:                                       ; preds = %for.body302
  %62 = load i32, i32* %padding346, align 4, !tbaa !18
  %add347 = add nsw i32 %62, %58
  %cmp350 = icmp sgt i32 %add347, %add307
  br i1 %cmp350, label %if.then352, label %endwrite.loopexit

if.then352:                                       ; preds = %if.else340
  br i1 %cmp353, label %if.then355, label %if.end357

if.then355:                                       ; preds = %if.then352
  %putchar575 = call i32 @putchar(i32 48)
  br label %if.end357

if.end357:                                        ; preds = %if.then355, %if.then352
  %63 = load i32*, i32** %data_col_index, align 8, !tbaa !14
  %arrayidx359 = getelementptr inbounds i32, i32* %63, i64 %indvars.iv
  store i32 -1, i32* %arrayidx359, align 4, !tbaa !12
  br label %if.end362

if.end362:                                        ; preds = %if.then310, %if.else335, %if.then330, %if.end357
  %indvars.iv.next = add nsw i64 %indvars.iv, 1
  %inc365 = add nuw nsw i32 %ipack.0584, 1
  %exitcond = icmp eq i32 %inc365, %pack_size
  br i1 %exitcond, label %for.inc367.loopexit, label %for.body302

for.inc367.loopexit:                              ; preds = %if.end362
  %64 = trunc i64 %indvars.iv.next to i32
  br label %for.inc367

for.inc367:                                       ; preds = %for.inc367.loopexit, %for.cond299.preheader
  %idata.2.lcssa = phi i32 [ %idata.1587, %for.cond299.preheader ], [ %64, %for.inc367.loopexit ]
  %indvars.iv.next618 = add nuw nsw i64 %indvars.iv617, 1
  %65 = load i32, i32* %rows, align 4, !tbaa !12
  %66 = sext i32 %65 to i64
  %cmp296 = icmp slt i64 %indvars.iv.next618, %66
  br i1 %cmp296, label %for.cond299.preheader, label %endwrite

endwrite.loopexit:                                ; preds = %if.else340
  %67 = trunc i64 %indvars.iv to i32
  br label %endwrite

endwrite:                                         ; preds = %for.inc367, %endwrite.loopexit, %for.cond295.preheader
  %idata.3 = phi i32 [ %idata.0593, %for.cond295.preheader ], [ %67, %endwrite.loopexit ], [ %idata.2.lcssa, %for.inc367 ]
  br i1 %cmp370, label %if.then372, label %if.end374

if.then372:                                       ; preds = %endwrite
  %putchar = call i32 @putchar(i32 10)
  br label %if.end374

if.end374:                                        ; preds = %if.then372, %endwrite
  %indvars.iv.next620 = add nuw i64 %indvars.iv619, 1
  %68 = load i32*, i32** %data_row_ptr, align 8, !tbaa !14
  %arrayidx284 = getelementptr inbounds i32, i32* %68, i64 %indvars.iv.next620
  store i32 %idata.3, i32* %arrayidx284, align 4, !tbaa !12
  %69 = load i32, i32* %51, align 4, !tbaa !9
  %70 = load i32, i32* %53, align 4, !tbaa !18
  %add289 = add nsw i32 %70, %69
  %71 = trunc i64 %indvars.iv.next620 to i32
  %mul290 = mul nsw i32 %71, %pack_size
  %cmp291 = icmp sgt i32 %add289, %mul290
  br i1 %cmp291, label %for.cond295.preheader, label %while.end.loopexit

while.end.loopexit:                               ; preds = %if.end374
  %72 = trunc i64 %indvars.iv.next620 to i32
  br label %while.end

while.end:                                        ; preds = %while.end.loopexit, %if.end275
  %irow.2.lcssa = phi i32 [ 0, %if.end275 ], [ %72, %while.end.loopexit ]
  br i1 %cmp100, label %if.then378, label %if.end380

if.then378:                                       ; preds = %while.end
  %73 = load i32, i32* %rows, align 4, !tbaa !12
  %call379 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([58 x i8], [58 x i8]* @.str.14, i64 0, i64 0), i32 %73, i32 %irow.2.lcssa)
  br label %if.end380

if.end380:                                        ; preds = %if.then378, %while.end
  call void @free(i8* %entries.0.in) #10
  call void @free(i8* nonnull %call122) #10
  %74 = load i32, i32* %nz_count_len, align 4, !tbaa !12
  %call381 = call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([19 x i8], [19 x i8]* @.str.15, i64 0, i64 0), i32 %74)
  %75 = load i32, i32* %rows, align 4, !tbaa !12
  store i32 %75, i32* %data_cols, align 4, !tbaa !12
  %add382 = add nuw nsw i32 %irow.2.lcssa, 1
  store i32 %add382, i32* %data_ptr_len, align 4, !tbaa !12
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %3) #10
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %2) #10
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %1) #10
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #10
  ret i32 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: nofree nounwind
declare dso_local noalias %struct._IO_FILE* @fopen(i8* nocapture readonly, i8* nocapture readonly) local_unnamed_addr #3

; Function Attrs: noreturn nounwind
declare dso_local void @exit(i32) local_unnamed_addr #4

declare dso_local i32 @mm_read_banner(%struct._IO_FILE*, [4 x i8]*) local_unnamed_addr #5

; Function Attrs: nofree nounwind
declare dso_local i32 @printf(i8* nocapture readonly, ...) local_unnamed_addr #3

declare dso_local i8* @mm_typecode_to_str(i8*) local_unnamed_addr #5

declare dso_local i32 @mm_read_mtx_crd_size(%struct._IO_FILE*, i32*, i32*, i32*) local_unnamed_addr #5

; Function Attrs: nofree nounwind
declare dso_local noalias i8* @malloc(i64) local_unnamed_addr #3

declare dso_local i32 @__isoc99_fscanf(%struct._IO_FILE*, i8*, ...) local_unnamed_addr #5

; Function Attrs: nofree nounwind
declare dso_local i32 @fclose(%struct._IO_FILE* nocapture) local_unnamed_addr #3

; Function Attrs: nofree
declare dso_local void @qsort(i8*, i64, i64, i32 (i8*, i8*)* nocapture) local_unnamed_addr #6

; Function Attrs: nofree nounwind
declare dso_local noalias i8* @calloc(i64, i64) local_unnamed_addr #3

; Function Attrs: nounwind
declare dso_local void @free(i8* nocapture) local_unnamed_addr #7

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: nofree nounwind
declare i32 @putchar(i32) local_unnamed_addr #8

; Function Attrs: nounwind readnone speculatable
declare float @llvm.ceil.f32(float) #9

; Function Attrs: nofree nounwind
declare i32 @puts(i8* nocapture readonly) local_unnamed_addr #8

attributes #0 = { norecurse nounwind readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #1 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }
attributes #3 = { nofree nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #4 = { noreturn nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #5 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #6 = { nofree "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #7 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #8 = { nofree nounwind }
attributes #9 = { nounwind readnone speculatable }
attributes #10 = { nounwind }
attributes #11 = { noreturn nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (https://gitlab.engr.illinois.edu/llvm/hpvm.git e24d6a1d89c07757edeea087e4ccf2ac27fe9fc7)"}
!2 = !{!3, !4, i64 0}
!3 = !{!"_mat_entry", !4, i64 0, !4, i64 4, !7, i64 8}
!4 = !{!"int", !5, i64 0}
!5 = !{!"omnipotent char", !6, i64 0}
!6 = !{!"Simple C/C++ TBAA"}
!7 = !{!"float", !5, i64 0}
!8 = !{!3, !4, i64 4}
!9 = !{!10, !4, i64 4}
!10 = !{!"_row_stats", !4, i64 0, !4, i64 4, !4, i64 8, !4, i64 12}
!11 = !{!5, !5, i64 0}
!12 = !{!4, !4, i64 0}
!13 = !{!3, !7, i64 8}
!14 = !{!15, !15, i64 0}
!15 = !{!"any pointer", !5, i64 0}
!16 = !{!10, !4, i64 0}
!17 = !{!10, !4, i64 8}
!18 = !{!10, !4, i64 12}
!19 = !{!7, !7, i64 0}

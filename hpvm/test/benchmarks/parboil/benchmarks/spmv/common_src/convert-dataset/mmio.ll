; ModuleID = 'src/hpvm/../../common_src/convert-dataset/mmio.c'
source_filename = "src/hpvm/../../common_src/convert-dataset/mmio.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, i8*, i8*, i8*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type { %struct._IO_marker*, %struct._IO_FILE*, i32 }

@.str = private unnamed_addr constant [2 x i8] c"r\00", align 1
@.str.1 = private unnamed_addr constant [60 x i8] c"mm_read_unsymetric: Could not process Matrix Market banner \00", align 1
@.str.2 = private unnamed_addr constant [15 x i8] c" in file [%s]\0A\00", align 1
@stderr = external dso_local local_unnamed_addr global %struct._IO_FILE*, align 8
@.str.3 = private unnamed_addr constant [42 x i8] c"Sorry, this application does not support \00", align 1
@.str.4 = private unnamed_addr constant [26 x i8] c"Market Market type: [%s]\0A\00", align 1
@.str.5 = private unnamed_addr constant [57 x i8] c"read_unsymmetric_sparse(): could not parse matrix size.\0A\00", align 1
@.str.6 = private unnamed_addr constant [11 x i8] c"%d %d %lg\0A\00", align 1
@.str.7 = private unnamed_addr constant [15 x i8] c"%s %s %s %s %s\00", align 1
@.str.8 = private unnamed_addr constant [15 x i8] c"%%MatrixMarket\00", align 1
@.str.9 = private unnamed_addr constant [7 x i8] c"matrix\00", align 1
@.str.10 = private unnamed_addr constant [11 x i8] c"coordinate\00", align 1
@.str.11 = private unnamed_addr constant [6 x i8] c"array\00", align 1
@.str.12 = private unnamed_addr constant [5 x i8] c"real\00", align 1
@.str.13 = private unnamed_addr constant [8 x i8] c"complex\00", align 1
@.str.14 = private unnamed_addr constant [8 x i8] c"pattern\00", align 1
@.str.15 = private unnamed_addr constant [8 x i8] c"integer\00", align 1
@.str.16 = private unnamed_addr constant [8 x i8] c"general\00", align 1
@.str.17 = private unnamed_addr constant [10 x i8] c"symmetric\00", align 1
@.str.18 = private unnamed_addr constant [10 x i8] c"hermitian\00", align 1
@.str.19 = private unnamed_addr constant [15 x i8] c"skew-symmetric\00", align 1
@.str.20 = private unnamed_addr constant [10 x i8] c"%d %d %d\0A\00", align 1
@.str.21 = private unnamed_addr constant [9 x i8] c"%d %d %d\00", align 1
@.str.22 = private unnamed_addr constant [6 x i8] c"%d %d\00", align 1
@.str.23 = private unnamed_addr constant [7 x i8] c"%d %d\0A\00", align 1
@.str.24 = private unnamed_addr constant [14 x i8] c"%d %d %lg %lg\00", align 1
@.str.25 = private unnamed_addr constant [6 x i8] c"stdin\00", align 1
@stdin = external dso_local local_unnamed_addr global %struct._IO_FILE*, align 8
@.str.26 = private unnamed_addr constant [7 x i8] c"%s %s\0A\00", align 1
@.str.27 = private unnamed_addr constant [7 x i8] c"stdout\00", align 1
@stdout = external dso_local local_unnamed_addr global %struct._IO_FILE*, align 8
@.str.28 = private unnamed_addr constant [2 x i8] c"w\00", align 1
@.str.29 = private unnamed_addr constant [4 x i8] c"%s \00", align 1
@.str.30 = private unnamed_addr constant [4 x i8] c"%s\0A\00", align 1
@.str.31 = private unnamed_addr constant [15 x i8] c"%d %d %20.16g\0A\00", align 1
@.str.32 = private unnamed_addr constant [23 x i8] c"%d %d %20.16g %20.16g\0A\00", align 1
@.str.33 = private unnamed_addr constant [12 x i8] c"%s %s %s %s\00", align 1

; Function Attrs: nounwind uwtable
define dso_local i32 @mm_read_unsymmetric_sparse(i8* %fname, i32* nocapture %M_, i32* nocapture %N_, i32* nocapture %nz_, double** nocapture %val_, i32** nocapture %I_, i32** nocapture %J_) local_unnamed_addr #0 {
entry:
  %matcode = alloca [4 x i8], align 1
  %M = alloca i32, align 4
  %N = alloca i32, align 4
  %nz = alloca i32, align 4
  %0 = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #12
  %1 = bitcast i32* %M to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %1) #12
  %2 = bitcast i32* %N to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %2) #12
  %3 = bitcast i32* %nz to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %3) #12
  %call = tail call %struct._IO_FILE* @fopen(i8* %fname, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str, i64 0, i64 0))
  %cmp = icmp eq %struct._IO_FILE* %call, null
  br i1 %cmp, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %call1 = call i32 @mm_read_banner(%struct._IO_FILE* nonnull %call, [4 x i8]* nonnull %matcode)
  %cmp2 = icmp eq i32 %call1, 0
  br i1 %cmp2, label %if.end6, label %if.then3

if.then3:                                         ; preds = %if.end
  %call4 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([60 x i8], [60 x i8]* @.str.1, i64 0, i64 0))
  %call5 = tail call i32 (i8*, ...) @printf(i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str.2, i64 0, i64 0), i8* %fname)
  br label %cleanup

if.end6:                                          ; preds = %if.end
  %arrayidx = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 2
  %4 = load i8, i8* %arrayidx, align 1, !tbaa !2
  %cmp7 = icmp eq i8 %4, 82
  %5 = load i8, i8* %0, align 1
  %cmp11 = icmp eq i8 %5, 77
  %or.cond = and i1 %cmp7, %cmp11
  br i1 %or.cond, label %land.lhs.true13, label %if.then18

land.lhs.true13:                                  ; preds = %if.end6
  %arrayidx14 = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 1
  %6 = load i8, i8* %arrayidx14, align 1, !tbaa !2
  %cmp16 = icmp eq i8 %6, 67
  br i1 %cmp16, label %if.end22, label %if.then18

if.then18:                                        ; preds = %land.lhs.true13, %if.end6
  %7 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !5
  %8 = tail call i64 @fwrite(i8* getelementptr inbounds ([42 x i8], [42 x i8]* @.str.3, i64 0, i64 0), i64 41, i64 1, %struct._IO_FILE* %7) #13
  %9 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !5
  %call20 = call i8* @mm_typecode_to_str(i8* nonnull %0)
  %call21 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %9, i8* getelementptr inbounds ([26 x i8], [26 x i8]* @.str.4, i64 0, i64 0), i8* %call20) #13
  br label %cleanup

if.end22:                                         ; preds = %land.lhs.true13
  %call23 = call i32 @mm_read_mtx_crd_size(%struct._IO_FILE* nonnull %call, i32* nonnull %M, i32* nonnull %N, i32* nonnull %nz)
  %cmp24 = icmp eq i32 %call23, 0
  br i1 %cmp24, label %if.end28, label %if.then26

if.then26:                                        ; preds = %if.end22
  %10 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !5
  %11 = call i64 @fwrite(i8* getelementptr inbounds ([57 x i8], [57 x i8]* @.str.5, i64 0, i64 0), i64 56, i64 1, %struct._IO_FILE* %10) #13
  br label %cleanup

if.end28:                                         ; preds = %if.end22
  %12 = load i32, i32* %M, align 4, !tbaa !7
  store i32 %12, i32* %M_, align 4, !tbaa !7
  %13 = load i32, i32* %N, align 4, !tbaa !7
  store i32 %13, i32* %N_, align 4, !tbaa !7
  %14 = load i32, i32* %nz, align 4, !tbaa !7
  store i32 %14, i32* %nz_, align 4, !tbaa !7
  %conv29 = sext i32 %14 to i64
  %mul = shl nsw i64 %conv29, 2
  %call30 = call noalias i8* @malloc(i64 %mul) #12
  %15 = bitcast i8* %call30 to i32*
  %call33 = call noalias i8* @malloc(i64 %mul) #12
  %16 = bitcast i8* %call33 to i32*
  %mul35 = shl nsw i64 %conv29, 3
  %call36 = call noalias i8* @malloc(i64 %mul35) #12
  %17 = bitcast i8* %call36 to double*
  %18 = bitcast double** %val_ to i8**
  store i8* %call36, i8** %18, align 8, !tbaa !5
  %19 = bitcast i32** %I_ to i8**
  store i8* %call30, i8** %19, align 8, !tbaa !5
  %20 = bitcast i32** %J_ to i8**
  store i8* %call33, i8** %20, align 8, !tbaa !5
  %21 = load i32, i32* %nz, align 4, !tbaa !7
  %cmp3778 = icmp sgt i32 %21, 0
  br i1 %cmp3778, label %for.body, label %for.end

for.body:                                         ; preds = %if.end28, %for.body
  %indvars.iv = phi i64 [ %indvars.iv.next, %for.body ], [ 0, %if.end28 ]
  %arrayidx39 = getelementptr inbounds i32, i32* %15, i64 %indvars.iv
  %arrayidx41 = getelementptr inbounds i32, i32* %16, i64 %indvars.iv
  %arrayidx43 = getelementptr inbounds double, double* %17, i64 %indvars.iv
  %call44 = call i32 (%struct._IO_FILE*, i8*, ...) @__isoc99_fscanf(%struct._IO_FILE* nonnull %call, i8* getelementptr inbounds ([11 x i8], [11 x i8]* @.str.6, i64 0, i64 0), i32* %arrayidx39, i32* %arrayidx41, double* %arrayidx43) #12
  %22 = load i32, i32* %arrayidx39, align 4, !tbaa !7
  %dec = add nsw i32 %22, -1
  store i32 %dec, i32* %arrayidx39, align 4, !tbaa !7
  %23 = load i32, i32* %arrayidx41, align 4, !tbaa !7
  %dec49 = add nsw i32 %23, -1
  store i32 %dec49, i32* %arrayidx41, align 4, !tbaa !7
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %24 = load i32, i32* %nz, align 4, !tbaa !7
  %25 = sext i32 %24 to i64
  %cmp37 = icmp slt i64 %indvars.iv.next, %25
  br i1 %cmp37, label %for.body, label %for.end

for.end:                                          ; preds = %for.body, %if.end28
  %call50 = call i32 @fclose(%struct._IO_FILE* nonnull %call)
  br label %cleanup

cleanup:                                          ; preds = %entry, %for.end, %if.then26, %if.then18, %if.then3
  %retval.0 = phi i32 [ -1, %if.then3 ], [ -1, %if.then26 ], [ 0, %for.end ], [ -1, %if.then18 ], [ -1, %entry ]
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %3) #12
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %2) #12
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %1) #12
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #12
  ret i32 %retval.0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nofree nounwind
declare dso_local noalias %struct._IO_FILE* @fopen(i8* nocapture readonly, i8* nocapture readonly) local_unnamed_addr #2

; Function Attrs: nounwind uwtable
define dso_local i32 @mm_read_banner(%struct._IO_FILE* nocapture %f, [4 x i8]* nocapture %matcode) local_unnamed_addr #0 {
entry:
  %line = alloca [1025 x i8], align 16
  %banner = alloca [64 x i8], align 16
  %mtx = alloca [64 x i8], align 16
  %crd = alloca [64 x i8], align 16
  %data_type = alloca [64 x i8], align 16
  %storage_scheme = alloca [64 x i8], align 16
  %0 = getelementptr inbounds [1025 x i8], [1025 x i8]* %line, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 1025, i8* nonnull %0) #12
  %1 = getelementptr inbounds [64 x i8], [64 x i8]* %banner, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 64, i8* nonnull %1) #12
  %2 = getelementptr inbounds [64 x i8], [64 x i8]* %mtx, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 64, i8* nonnull %2) #12
  %3 = getelementptr inbounds [64 x i8], [64 x i8]* %crd, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 64, i8* nonnull %3) #12
  %4 = getelementptr inbounds [64 x i8], [64 x i8]* %data_type, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 64, i8* nonnull %4) #12
  %5 = getelementptr inbounds [64 x i8], [64 x i8]* %storage_scheme, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 64, i8* nonnull %5) #12
  %arrayidx = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 2
  store i8 32, i8* %arrayidx, align 1, !tbaa !2
  %arrayidx1 = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 1
  store i8 32, i8* %arrayidx1, align 1, !tbaa !2
  %arrayidx2 = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 0
  store i8 32, i8* %arrayidx2, align 1, !tbaa !2
  %arrayidx3 = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 3
  store i8 71, i8* %arrayidx3, align 1, !tbaa !2
  %call = call i8* @fgets(i8* nonnull %0, i32 1025, %struct._IO_FILE* %f)
  %cmp = icmp eq i8* %call, null
  br i1 %cmp, label %cleanup, label %if.end

if.end:                                           ; preds = %entry
  %call10 = call i32 (i8*, i8*, ...) @__isoc99_sscanf(i8* nonnull %0, i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str.7, i64 0, i64 0), i8* nonnull %1, i8* nonnull %2, i8* nonnull %3, i8* nonnull %4, i8* nonnull %5) #12
  %cmp11 = icmp eq i32 %call10, 5
  br i1 %cmp11, label %for.cond.preheader, label %cleanup

for.cond.preheader:                               ; preds = %if.end
  %6 = load i8, i8* %2, align 16, !tbaa !2
  %cmp152306 = icmp eq i8 %6, 0
  br i1 %cmp152306, label %for.cond22.preheader, label %for.inc.lr.ph

for.inc.lr.ph:                                    ; preds = %for.cond.preheader
  %call17 = tail call i32** @__ctype_tolower_loc() #14
  br label %for.inc

for.cond22.preheader:                             ; preds = %for.inc, %for.cond.preheader
  %7 = load i8, i8* %3, align 16, !tbaa !2
  %cmp242304 = icmp eq i8 %7, 0
  br i1 %cmp242304, label %for.cond38.preheader, label %for.inc27.lr.ph

for.inc27.lr.ph:                                  ; preds = %for.cond22.preheader
  %call29 = tail call i32** @__ctype_tolower_loc() #14
  br label %for.inc27

for.inc:                                          ; preds = %for.inc.lr.ph, %for.inc
  %8 = phi i8 [ %6, %for.inc.lr.ph ], [ %11, %for.inc ]
  %p.02307 = phi i8* [ %2, %for.inc.lr.ph ], [ %incdec.ptr, %for.inc ]
  %9 = load i32*, i32** %call17, align 8, !tbaa !5
  %idxprom = sext i8 %8 to i64
  %arrayidx19 = getelementptr inbounds i32, i32* %9, i64 %idxprom
  %10 = load i32, i32* %arrayidx19, align 4, !tbaa !7
  %conv20 = trunc i32 %10 to i8
  store i8 %conv20, i8* %p.02307, align 1, !tbaa !2
  %incdec.ptr = getelementptr inbounds i8, i8* %p.02307, i64 1
  %11 = load i8, i8* %incdec.ptr, align 1, !tbaa !2
  %cmp15 = icmp eq i8 %11, 0
  br i1 %cmp15, label %for.cond22.preheader, label %for.inc

for.cond38.preheader:                             ; preds = %for.inc27, %for.cond22.preheader
  %12 = load i8, i8* %4, align 16, !tbaa !2
  %cmp402302 = icmp eq i8 %12, 0
  br i1 %cmp402302, label %for.cond54.preheader, label %for.inc43.lr.ph

for.inc43.lr.ph:                                  ; preds = %for.cond38.preheader
  %call45 = tail call i32** @__ctype_tolower_loc() #14
  br label %for.inc43

for.inc27:                                        ; preds = %for.inc27.lr.ph, %for.inc27
  %13 = phi i8 [ %7, %for.inc27.lr.ph ], [ %16, %for.inc27 ]
  %p.12305 = phi i8* [ %3, %for.inc27.lr.ph ], [ %incdec.ptr35, %for.inc27 ]
  %14 = load i32*, i32** %call29, align 8, !tbaa !5
  %idxprom31 = sext i8 %13 to i64
  %arrayidx32 = getelementptr inbounds i32, i32* %14, i64 %idxprom31
  %15 = load i32, i32* %arrayidx32, align 4, !tbaa !7
  %conv34 = trunc i32 %15 to i8
  store i8 %conv34, i8* %p.12305, align 1, !tbaa !2
  %incdec.ptr35 = getelementptr inbounds i8, i8* %p.12305, i64 1
  %16 = load i8, i8* %incdec.ptr35, align 1, !tbaa !2
  %cmp24 = icmp eq i8 %16, 0
  br i1 %cmp24, label %for.cond38.preheader, label %for.inc27

for.cond54.preheader:                             ; preds = %for.inc43, %for.cond38.preheader
  %17 = load i8, i8* %5, align 16, !tbaa !2
  %cmp562300 = icmp eq i8 %17, 0
  br i1 %cmp562300, label %cond.false220, label %for.inc59.lr.ph

for.inc59.lr.ph:                                  ; preds = %for.cond54.preheader
  %call61 = tail call i32** @__ctype_tolower_loc() #14
  br label %for.inc59

for.inc43:                                        ; preds = %for.inc43.lr.ph, %for.inc43
  %18 = phi i8 [ %12, %for.inc43.lr.ph ], [ %21, %for.inc43 ]
  %p.22303 = phi i8* [ %4, %for.inc43.lr.ph ], [ %incdec.ptr51, %for.inc43 ]
  %19 = load i32*, i32** %call45, align 8, !tbaa !5
  %idxprom47 = sext i8 %18 to i64
  %arrayidx48 = getelementptr inbounds i32, i32* %19, i64 %idxprom47
  %20 = load i32, i32* %arrayidx48, align 4, !tbaa !7
  %conv50 = trunc i32 %20 to i8
  store i8 %conv50, i8* %p.22303, align 1, !tbaa !2
  %incdec.ptr51 = getelementptr inbounds i8, i8* %p.22303, i64 1
  %21 = load i8, i8* %incdec.ptr51, align 1, !tbaa !2
  %cmp40 = icmp eq i8 %21, 0
  br i1 %cmp40, label %for.cond54.preheader, label %for.inc43

for.inc59:                                        ; preds = %for.inc59.lr.ph, %for.inc59
  %22 = phi i8 [ %17, %for.inc59.lr.ph ], [ %25, %for.inc59 ]
  %p.32301 = phi i8* [ %5, %for.inc59.lr.ph ], [ %incdec.ptr67, %for.inc59 ]
  %23 = load i32*, i32** %call61, align 8, !tbaa !5
  %idxprom63 = sext i8 %22 to i64
  %arrayidx64 = getelementptr inbounds i32, i32* %23, i64 %idxprom63
  %24 = load i32, i32* %arrayidx64, align 4, !tbaa !7
  %conv66 = trunc i32 %24 to i8
  store i8 %conv66, i8* %p.32301, align 1, !tbaa !2
  %incdec.ptr67 = getelementptr inbounds i8, i8* %p.32301, i64 1
  %25 = load i8, i8* %incdec.ptr67, align 1, !tbaa !2
  %cmp56 = icmp eq i8 %25, 0
  br i1 %cmp56, label %cond.false220, label %for.inc59

cond.false220:                                    ; preds = %for.inc59, %for.cond54.preheader
  %bcmp = call i32 @bcmp(i8* nonnull %1, i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str.8, i64 0, i64 0), i64 14)
  %cmp225 = icmp eq i32 %bcmp, 0
  br i1 %cmp225, label %cond.end388, label %cleanup

cond.end388:                                      ; preds = %cond.false220
  %bcmp2290 = call i32 @bcmp(i8* nonnull %2, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.9, i64 0, i64 0), i64 7)
  %cmp390 = icmp eq i32 %bcmp2290, 0
  br i1 %cmp390, label %if.end393, label %cleanup

if.end393:                                        ; preds = %cond.end388
  store i8 77, i8* %arrayidx2, align 1, !tbaa !2
  %bcmp2291 = call i32 @bcmp(i8* nonnull %3, i8* getelementptr inbounds ([11 x i8], [11 x i8]* @.str.10, i64 0, i64 0), i64 11)
  %cmp556 = icmp eq i32 %bcmp2291, 0
  br i1 %cmp556, label %if.end727, label %cond.end719

cond.end719:                                      ; preds = %if.end393
  %bcmp2292 = call i32 @bcmp(i8* nonnull %3, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.11, i64 0, i64 0), i64 6)
  %cmp721 = icmp eq i32 %bcmp2292, 0
  br i1 %cmp721, label %if.end727, label %cleanup

if.end727:                                        ; preds = %cond.end719, %if.end393
  %storemerge = phi i8 [ 67, %if.end393 ], [ 65, %cond.end719 ]
  store i8 %storemerge, i8* %arrayidx1, align 1, !tbaa !2
  %bcmp2293 = call i32 @bcmp(i8* nonnull %4, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.12, i64 0, i64 0), i64 5)
  %cmp889 = icmp eq i32 %bcmp2293, 0
  br i1 %cmp889, label %cond.end1555, label %cond.end1053

cond.end1053:                                     ; preds = %if.end727
  %26 = bitcast [64 x i8]* %data_type to i64*
  %lhsv = load i64, i64* %26, align 16
  switch i64 %lhsv, label %cleanup [
    i64 33888513622372195, label %cond.end1555
    i64 31088027509219696, label %if.then1223
    i64 32199642103180905, label %if.then1389
  ]

if.then1223:                                      ; preds = %cond.end1053
  br label %cond.end1555

if.then1389:                                      ; preds = %cond.end1053
  br label %cond.end1555

cond.end1555:                                     ; preds = %cond.end1053, %if.end727, %if.then1223, %if.then1389
  %.sink = phi i8 [ 80, %if.then1223 ], [ 73, %if.then1389 ], [ 82, %if.end727 ], [ 67, %cond.end1053 ]
  store i8 %.sink, i8* %arrayidx, align 1, !tbaa !2
  %27 = bitcast [64 x i8]* %storage_scheme to i64*
  %lhsv2296 = load i64, i64* %27, align 16
  %28 = icmp eq i64 %lhsv2296, 30506441440650599
  br i1 %28, label %if.then1559, label %cond.end1721

if.then1559:                                      ; preds = %cond.end1555
  store i8 71, i8* %arrayidx3, align 1, !tbaa !2
  br label %cleanup

cond.end1721:                                     ; preds = %cond.end1555
  %bcmp2297 = call i32 @bcmp(i8* nonnull %5, i8* getelementptr inbounds ([10 x i8], [10 x i8]* @.str.17, i64 0, i64 0), i64 10)
  %cmp1723 = icmp eq i32 %bcmp2297, 0
  br i1 %cmp1723, label %if.then1725, label %cond.end1887

if.then1725:                                      ; preds = %cond.end1721
  store i8 83, i8* %arrayidx3, align 1, !tbaa !2
  br label %cleanup

cond.end1887:                                     ; preds = %cond.end1721
  %bcmp2298 = call i32 @bcmp(i8* nonnull %5, i8* getelementptr inbounds ([10 x i8], [10 x i8]* @.str.18, i64 0, i64 0), i64 10)
  %cmp1889 = icmp eq i32 %bcmp2298, 0
  br i1 %cmp1889, label %if.then1891, label %cond.end2053

if.then1891:                                      ; preds = %cond.end1887
  store i8 72, i8* %arrayidx3, align 1, !tbaa !2
  br label %cleanup

cond.end2053:                                     ; preds = %cond.end1887
  %bcmp2299 = call i32 @bcmp(i8* nonnull %5, i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str.19, i64 0, i64 0), i64 15)
  %cmp2055 = icmp eq i32 %bcmp2299, 0
  br i1 %cmp2055, label %if.then2057, label %cleanup

if.then2057:                                      ; preds = %cond.end2053
  store i8 75, i8* %arrayidx3, align 1, !tbaa !2
  br label %cleanup

cleanup:                                          ; preds = %cond.end1053, %if.then1559, %if.then1891, %if.then2057, %if.then1725, %cond.end2053, %cond.end719, %cond.end388, %cond.false220, %if.end, %entry
  %retval.0 = phi i32 [ 12, %entry ], [ 12, %if.end ], [ 14, %cond.false220 ], [ 15, %cond.end388 ], [ 15, %cond.end719 ], [ 15, %cond.end2053 ], [ 0, %if.then1725 ], [ 0, %if.then2057 ], [ 0, %if.then1891 ], [ 0, %if.then1559 ], [ 15, %cond.end1053 ]
  call void @llvm.lifetime.end.p0i8(i64 64, i8* nonnull %5) #12
  call void @llvm.lifetime.end.p0i8(i64 64, i8* nonnull %4) #12
  call void @llvm.lifetime.end.p0i8(i64 64, i8* nonnull %3) #12
  call void @llvm.lifetime.end.p0i8(i64 64, i8* nonnull %2) #12
  call void @llvm.lifetime.end.p0i8(i64 64, i8* nonnull %1) #12
  call void @llvm.lifetime.end.p0i8(i64 1025, i8* nonnull %0) #12
  ret i32 %retval.0
}

; Function Attrs: nofree nounwind
declare dso_local i32 @printf(i8* nocapture readonly, ...) local_unnamed_addr #2

; Function Attrs: nofree nounwind
declare dso_local i32 @fprintf(%struct._IO_FILE* nocapture, i8* nocapture readonly, ...) local_unnamed_addr #2

; Function Attrs: nounwind uwtable
define dso_local i8* @mm_typecode_to_str(i8* nocapture readonly %matcode) local_unnamed_addr #0 {
entry:
  %buffer = alloca [1025 x i8], align 16
  %0 = getelementptr inbounds [1025 x i8], [1025 x i8]* %buffer, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 1025, i8* nonnull %0) #12
  %arrayidx3 = getelementptr inbounds i8, i8* %matcode, i64 1
  %1 = load i8, i8* %arrayidx3, align 1, !tbaa !2
  switch i8 %1, label %cleanup [
    i8 67, label %if.end18
    i8 65, label %if.then14
  ]

if.then14:                                        ; preds = %entry
  br label %if.end18

if.end18:                                         ; preds = %entry, %if.then14
  %types.sroa.4.0 = phi i8* [ getelementptr inbounds ([6 x i8], [6 x i8]* @.str.11, i64 0, i64 0), %if.then14 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str.10, i64 0, i64 0), %entry ]
  %arrayidx19 = getelementptr inbounds i8, i8* %matcode, i64 2
  %2 = load i8, i8* %arrayidx19, align 1, !tbaa !2
  switch i8 %2, label %cleanup [
    i8 82, label %if.end50
    i8 67, label %if.then30
    i8 80, label %if.then37
    i8 73, label %if.then44
  ]

if.then30:                                        ; preds = %if.end18
  br label %if.end50

if.then37:                                        ; preds = %if.end18
  br label %if.end50

if.then44:                                        ; preds = %if.end18
  br label %if.end50

if.end50:                                         ; preds = %if.end18, %if.then30, %if.then44, %if.then37
  %types.sroa.7.0 = phi i8* [ getelementptr inbounds ([8 x i8], [8 x i8]* @.str.13, i64 0, i64 0), %if.then30 ], [ getelementptr inbounds ([8 x i8], [8 x i8]* @.str.14, i64 0, i64 0), %if.then37 ], [ getelementptr inbounds ([8 x i8], [8 x i8]* @.str.15, i64 0, i64 0), %if.then44 ], [ getelementptr inbounds ([5 x i8], [5 x i8]* @.str.12, i64 0, i64 0), %if.end18 ]
  %arrayidx51 = getelementptr inbounds i8, i8* %matcode, i64 3
  %3 = load i8, i8* %arrayidx51, align 1, !tbaa !2
  switch i8 %3, label %cleanup [
    i8 71, label %if.end82
    i8 83, label %if.then62
    i8 72, label %if.then69
    i8 75, label %if.then76
  ]

if.then62:                                        ; preds = %if.end50
  br label %if.end82

if.then69:                                        ; preds = %if.end50
  br label %if.end82

if.then76:                                        ; preds = %if.end50
  br label %if.end82

if.end82:                                         ; preds = %if.end50, %if.then62, %if.then76, %if.then69
  %types.sroa.12.0 = phi i8* [ getelementptr inbounds ([10 x i8], [10 x i8]* @.str.17, i64 0, i64 0), %if.then62 ], [ getelementptr inbounds ([10 x i8], [10 x i8]* @.str.18, i64 0, i64 0), %if.then69 ], [ getelementptr inbounds ([15 x i8], [15 x i8]* @.str.19, i64 0, i64 0), %if.then76 ], [ getelementptr inbounds ([8 x i8], [8 x i8]* @.str.16, i64 0, i64 0), %if.end50 ]
  %call = call i32 (i8*, i8*, ...) @sprintf(i8* nonnull %0, i8* getelementptr inbounds ([12 x i8], [12 x i8]* @.str.33, i64 0, i64 0), i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.9, i64 0, i64 0), i8* %types.sroa.4.0, i8* %types.sroa.7.0, i8* nonnull %types.sroa.12.0) #12
  %call88 = call i8* @mm_strdup(i8* nonnull %0)
  br label %cleanup

cleanup:                                          ; preds = %if.end50, %if.end18, %entry, %if.end82
  %retval.0 = phi i8* [ %call88, %if.end82 ], [ null, %entry ], [ null, %if.end18 ], [ null, %if.end50 ]
  call void @llvm.lifetime.end.p0i8(i64 1025, i8* nonnull %0) #12
  ret i8* %retval.0
}

; Function Attrs: nounwind uwtable
define dso_local i32 @mm_read_mtx_crd_size(%struct._IO_FILE* %f, i32* %M, i32* %N, i32* %nz) local_unnamed_addr #0 {
entry:
  %line = alloca [1025 x i8], align 16
  %0 = getelementptr inbounds [1025 x i8], [1025 x i8]* %line, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 1025, i8* nonnull %0) #12
  store i32 0, i32* %nz, align 4, !tbaa !7
  store i32 0, i32* %N, align 4, !tbaa !7
  store i32 0, i32* %M, align 4, !tbaa !7
  br label %do.body

do.body:                                          ; preds = %do.cond, %entry
  %call = call i8* @fgets(i8* nonnull %0, i32 1025, %struct._IO_FILE* %f)
  %cmp = icmp eq i8* %call, null
  br i1 %cmp, label %cleanup, label %do.cond

do.cond:                                          ; preds = %do.body
  %1 = load i8, i8* %0, align 16, !tbaa !2
  %cmp1 = icmp eq i8 %1, 37
  br i1 %cmp1, label %do.body, label %do.end

do.end:                                           ; preds = %do.cond
  %call4 = call i32 (i8*, i8*, ...) @__isoc99_sscanf(i8* nonnull %0, i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.21, i64 0, i64 0), i32* %M, i32* %N, i32* %nz) #12
  %cmp5 = icmp eq i32 %call4, 3
  br i1 %cmp5, label %cleanup, label %do.body8

do.body8:                                         ; preds = %do.end, %do.body8
  %call9 = call i32 (%struct._IO_FILE*, i8*, ...) @__isoc99_fscanf(%struct._IO_FILE* %f, i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.21, i64 0, i64 0), i32* %M, i32* %N, i32* %nz) #12
  switch i32 %call9, label %do.body8 [
    i32 -1, label %cleanup.loopexit
    i32 3, label %cleanup
  ]

cleanup.loopexit:                                 ; preds = %do.body8
  br label %cleanup

cleanup:                                          ; preds = %do.body, %do.body8, %cleanup.loopexit, %do.end
  %retval.0 = phi i32 [ 0, %do.end ], [ 12, %cleanup.loopexit ], [ 0, %do.body8 ], [ 12, %do.body ]
  call void @llvm.lifetime.end.p0i8(i64 1025, i8* nonnull %0) #12
  ret i32 %retval.0
}

; Function Attrs: nofree nounwind
declare dso_local noalias i8* @malloc(i64) local_unnamed_addr #2

declare dso_local i32 @__isoc99_fscanf(%struct._IO_FILE*, i8*, ...) local_unnamed_addr #3

; Function Attrs: nofree nounwind
declare dso_local i32 @fclose(%struct._IO_FILE* nocapture) local_unnamed_addr #2

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: norecurse nounwind readonly uwtable
define dso_local i32 @mm_is_valid(i8* nocapture readonly %matcode) local_unnamed_addr #4 {
entry:
  %0 = load i8, i8* %matcode, align 1, !tbaa !2
  %cmp = icmp eq i8 %0, 77
  br i1 %cmp, label %if.end, label %return

if.end:                                           ; preds = %entry
  %arrayidx2 = getelementptr inbounds i8, i8* %matcode, i64 1
  %1 = load i8, i8* %arrayidx2, align 1, !tbaa !2
  %cmp4 = icmp eq i8 %1, 65
  br i1 %cmp4, label %land.lhs.true, label %if.end11

land.lhs.true:                                    ; preds = %if.end
  %arrayidx6 = getelementptr inbounds i8, i8* %matcode, i64 2
  %2 = load i8, i8* %arrayidx6, align 1, !tbaa !2
  %cmp8 = icmp eq i8 %2, 80
  br i1 %cmp8, label %return, label %if.end11

if.end11:                                         ; preds = %land.lhs.true, %if.end
  %arrayidx12 = getelementptr inbounds i8, i8* %matcode, i64 2
  %3 = load i8, i8* %arrayidx12, align 1, !tbaa !2
  switch i8 %3, label %if.end37 [
    i8 82, label %land.lhs.true16
    i8 80, label %land.lhs.true27
  ]

land.lhs.true16:                                  ; preds = %if.end11
  %arrayidx17 = getelementptr inbounds i8, i8* %matcode, i64 3
  %4 = load i8, i8* %arrayidx17, align 1, !tbaa !2
  %cmp19 = icmp eq i8 %4, 72
  br i1 %cmp19, label %return, label %if.end37

land.lhs.true27:                                  ; preds = %if.end11
  %arrayidx28 = getelementptr inbounds i8, i8* %matcode, i64 3
  %5 = load i8, i8* %arrayidx28, align 1, !tbaa !2
  switch i8 %5, label %if.end37 [
    i8 72, label %return
    i8 75, label %return
  ]

if.end37:                                         ; preds = %if.end11, %land.lhs.true16, %land.lhs.true27
  br label %return

return:                                           ; preds = %land.lhs.true27, %land.lhs.true27, %land.lhs.true16, %land.lhs.true, %entry, %if.end37
  %retval.0 = phi i32 [ 1, %if.end37 ], [ 0, %entry ], [ 0, %land.lhs.true ], [ 0, %land.lhs.true16 ], [ 0, %land.lhs.true27 ], [ 0, %land.lhs.true27 ]
  ret i32 %retval.0
}

; Function Attrs: nofree nounwind
declare dso_local i8* @fgets(i8*, i32, %struct._IO_FILE* nocapture) local_unnamed_addr #2

; Function Attrs: nofree nounwind
declare dso_local i32 @__isoc99_sscanf(i8* nocapture readonly, i8* nocapture readonly, ...) local_unnamed_addr #2

; Function Attrs: nounwind readnone
declare dso_local i32** @__ctype_tolower_loc() local_unnamed_addr #5

; Function Attrs: argmemonly nofree nounwind readonly
declare dso_local i64 @strlen(i8* nocapture) local_unnamed_addr #6

; Function Attrs: nofree nounwind readonly
declare dso_local i32 @strcmp(i8* nocapture, i8* nocapture) local_unnamed_addr #7

; Function Attrs: nofree nounwind uwtable
define dso_local i32 @mm_write_mtx_crd_size(%struct._IO_FILE* nocapture %f, i32 %M, i32 %N, i32 %nz) local_unnamed_addr #8 {
entry:
  %call = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %f, i8* getelementptr inbounds ([10 x i8], [10 x i8]* @.str.20, i64 0, i64 0), i32 %M, i32 %N, i32 %nz)
  %cmp = icmp eq i32 %call, 3
  %. = select i1 %cmp, i32 0, i32 17
  ret i32 %.
}

; Function Attrs: nounwind uwtable
define dso_local i32 @mm_read_mtx_array_size(%struct._IO_FILE* %f, i32* %M, i32* %N) local_unnamed_addr #0 {
entry:
  %line = alloca [1025 x i8], align 16
  %0 = getelementptr inbounds [1025 x i8], [1025 x i8]* %line, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 1025, i8* nonnull %0) #12
  store i32 0, i32* %N, align 4, !tbaa !7
  store i32 0, i32* %M, align 4, !tbaa !7
  br label %do.body

do.body:                                          ; preds = %do.cond, %entry
  %call = call i8* @fgets(i8* nonnull %0, i32 1025, %struct._IO_FILE* %f)
  %cmp = icmp eq i8* %call, null
  br i1 %cmp, label %cleanup, label %do.cond

do.cond:                                          ; preds = %do.body
  %1 = load i8, i8* %0, align 16, !tbaa !2
  %cmp1 = icmp eq i8 %1, 37
  br i1 %cmp1, label %do.body, label %do.end

do.end:                                           ; preds = %do.cond
  %call4 = call i32 (i8*, i8*, ...) @__isoc99_sscanf(i8* nonnull %0, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.22, i64 0, i64 0), i32* %M, i32* %N) #12
  %cmp5 = icmp eq i32 %call4, 2
  br i1 %cmp5, label %cleanup, label %do.body8

do.body8:                                         ; preds = %do.end, %do.body8
  %call9 = call i32 (%struct._IO_FILE*, i8*, ...) @__isoc99_fscanf(%struct._IO_FILE* %f, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.22, i64 0, i64 0), i32* %M, i32* %N) #12
  switch i32 %call9, label %do.body8 [
    i32 -1, label %cleanup.loopexit
    i32 2, label %cleanup
  ]

cleanup.loopexit:                                 ; preds = %do.body8
  br label %cleanup

cleanup:                                          ; preds = %do.body, %do.body8, %cleanup.loopexit, %do.end
  %retval.0 = phi i32 [ 0, %do.end ], [ 12, %cleanup.loopexit ], [ 0, %do.body8 ], [ 12, %do.body ]
  call void @llvm.lifetime.end.p0i8(i64 1025, i8* nonnull %0) #12
  ret i32 %retval.0
}

; Function Attrs: nofree nounwind uwtable
define dso_local i32 @mm_write_mtx_array_size(%struct._IO_FILE* nocapture %f, i32 %M, i32 %N) local_unnamed_addr #8 {
entry:
  %call = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %f, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.23, i64 0, i64 0), i32 %M, i32 %N)
  %cmp = icmp eq i32 %call, 2
  %. = select i1 %cmp, i32 0, i32 17
  ret i32 %.
}

; Function Attrs: nounwind uwtable
define dso_local i32 @mm_read_mtx_crd_data(%struct._IO_FILE* %f, i32 %M, i32 %N, i32 %nz, i32* %I, i32* %J, double* %val, i8* nocapture readonly %matcode) local_unnamed_addr #0 {
entry:
  %arrayidx = getelementptr inbounds i8, i8* %matcode, i64 2
  %0 = load i8, i8* %arrayidx, align 1, !tbaa !2
  switch i8 %0, label %cleanup [
    i8 67, label %for.cond.preheader
    i8 82, label %for.cond20.preheader
    i8 80, label %for.cond44.preheader
  ]

for.cond44.preheader:                             ; preds = %entry
  %cmp45102 = icmp sgt i32 %nz, 0
  br i1 %cmp45102, label %for.body47.preheader, label %cleanup

for.body47.preheader:                             ; preds = %for.cond44.preheader
  %wide.trip.count118 = zext i32 %nz to i64
  br label %for.body47

for.cond20.preheader:                             ; preds = %entry
  %cmp2198 = icmp sgt i32 %nz, 0
  br i1 %cmp2198, label %for.body23.preheader, label %cleanup

for.body23.preheader:                             ; preds = %for.cond20.preheader
  %wide.trip.count114 = zext i32 %nz to i64
  br label %for.body23

for.cond.preheader:                               ; preds = %entry
  %cmp295 = icmp sgt i32 %nz, 0
  br i1 %cmp295, label %for.body.preheader, label %cleanup

for.body.preheader:                               ; preds = %for.cond.preheader
  %wide.trip.count = zext i32 %nz to i64
  br label %for.body

for.cond:                                         ; preds = %for.body
  %exitcond = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond, label %cleanup, label %for.body

for.body:                                         ; preds = %for.cond, %for.body.preheader
  %indvars.iv = phi i64 [ 0, %for.body.preheader ], [ %indvars.iv.next, %for.cond ]
  %arrayidx4 = getelementptr inbounds i32, i32* %I, i64 %indvars.iv
  %arrayidx6 = getelementptr inbounds i32, i32* %J, i64 %indvars.iv
  %1 = shl nuw nsw i64 %indvars.iv, 1
  %arrayidx8 = getelementptr inbounds double, double* %val, i64 %1
  %2 = or i64 %1, 1
  %arrayidx11 = getelementptr inbounds double, double* %val, i64 %2
  %call = tail call i32 (%struct._IO_FILE*, i8*, ...) @__isoc99_fscanf(%struct._IO_FILE* %f, i8* getelementptr inbounds ([14 x i8], [14 x i8]* @.str.24, i64 0, i64 0), i32* %arrayidx4, i32* %arrayidx6, double* %arrayidx8, double* nonnull %arrayidx11) #12
  %cmp12 = icmp eq i32 %call, 4
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  br i1 %cmp12, label %for.cond, label %cleanup

for.cond20:                                       ; preds = %for.body23
  %exitcond115 = icmp eq i64 %indvars.iv.next113, %wide.trip.count114
  br i1 %exitcond115, label %cleanup, label %for.body23

for.body23:                                       ; preds = %for.cond20, %for.body23.preheader
  %indvars.iv112 = phi i64 [ 0, %for.body23.preheader ], [ %indvars.iv.next113, %for.cond20 ]
  %arrayidx25 = getelementptr inbounds i32, i32* %I, i64 %indvars.iv112
  %arrayidx27 = getelementptr inbounds i32, i32* %J, i64 %indvars.iv112
  %arrayidx29 = getelementptr inbounds double, double* %val, i64 %indvars.iv112
  %call30 = tail call i32 (%struct._IO_FILE*, i8*, ...) @__isoc99_fscanf(%struct._IO_FILE* %f, i8* getelementptr inbounds ([11 x i8], [11 x i8]* @.str.6, i64 0, i64 0), i32* %arrayidx25, i32* %arrayidx27, double* %arrayidx29) #12
  %cmp31 = icmp eq i32 %call30, 3
  %indvars.iv.next113 = add nuw nsw i64 %indvars.iv112, 1
  br i1 %cmp31, label %for.cond20, label %cleanup

for.cond44:                                       ; preds = %for.body47
  %exitcond119 = icmp eq i64 %indvars.iv.next117, %wide.trip.count118
  br i1 %exitcond119, label %cleanup, label %for.body47

for.body47:                                       ; preds = %for.cond44, %for.body47.preheader
  %indvars.iv116 = phi i64 [ 0, %for.body47.preheader ], [ %indvars.iv.next117, %for.cond44 ]
  %arrayidx49 = getelementptr inbounds i32, i32* %I, i64 %indvars.iv116
  %arrayidx51 = getelementptr inbounds i32, i32* %J, i64 %indvars.iv116
  %call52 = tail call i32 (%struct._IO_FILE*, i8*, ...) @__isoc99_fscanf(%struct._IO_FILE* %f, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.22, i64 0, i64 0), i32* %arrayidx49, i32* %arrayidx51) #12
  %cmp53 = icmp eq i32 %call52, 2
  %indvars.iv.next117 = add nuw nsw i64 %indvars.iv116, 1
  br i1 %cmp53, label %for.cond44, label %cleanup

cleanup:                                          ; preds = %for.body47, %for.cond44, %for.body23, %for.cond20, %for.body, %for.cond, %for.cond44.preheader, %for.cond20.preheader, %for.cond.preheader, %entry
  %retval.0 = phi i32 [ 15, %entry ], [ 0, %for.cond.preheader ], [ 0, %for.cond20.preheader ], [ 0, %for.cond44.preheader ], [ 12, %for.body ], [ 0, %for.cond ], [ 12, %for.body23 ], [ 0, %for.cond20 ], [ 12, %for.body47 ], [ 0, %for.cond44 ]
  ret i32 %retval.0
}

; Function Attrs: nounwind uwtable
define dso_local i32 @mm_read_mtx_crd_entry(%struct._IO_FILE* %f, i32* %I, i32* %J, double* %real, double* %imag, i8* nocapture readonly %matcode) local_unnamed_addr #0 {
entry:
  %arrayidx = getelementptr inbounds i8, i8* %matcode, i64 2
  %0 = load i8, i8* %arrayidx, align 1, !tbaa !2
  switch i8 %0, label %return [
    i8 67, label %if.then
    i8 82, label %if.then9
    i8 80, label %if.then20
  ]

if.then:                                          ; preds = %entry
  %call = tail call i32 (%struct._IO_FILE*, i8*, ...) @__isoc99_fscanf(%struct._IO_FILE* %f, i8* getelementptr inbounds ([14 x i8], [14 x i8]* @.str.24, i64 0, i64 0), i32* %I, i32* %J, double* %real, double* %imag) #12
  %cmp2 = icmp eq i32 %call, 4
  br i1 %cmp2, label %if.end29, label %return

if.then9:                                         ; preds = %entry
  %call10 = tail call i32 (%struct._IO_FILE*, i8*, ...) @__isoc99_fscanf(%struct._IO_FILE* %f, i8* getelementptr inbounds ([11 x i8], [11 x i8]* @.str.6, i64 0, i64 0), i32* %I, i32* %J, double* %real) #12
  %cmp11 = icmp eq i32 %call10, 3
  br i1 %cmp11, label %if.end29, label %return

if.then20:                                        ; preds = %entry
  %call21 = tail call i32 (%struct._IO_FILE*, i8*, ...) @__isoc99_fscanf(%struct._IO_FILE* %f, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.22, i64 0, i64 0), i32* %I, i32* %J) #12
  %cmp22 = icmp eq i32 %call21, 2
  br i1 %cmp22, label %if.end29, label %return

if.end29:                                         ; preds = %if.then, %if.then9, %if.then20
  br label %return

return:                                           ; preds = %entry, %if.then20, %if.then9, %if.then, %if.end29
  %retval.0 = phi i32 [ 0, %if.end29 ], [ 12, %if.then ], [ 12, %if.then9 ], [ 12, %if.then20 ], [ 15, %entry ]
  ret i32 %retval.0
}

; Function Attrs: nounwind uwtable
define dso_local i32 @mm_read_mtx_crd(i8* nocapture readonly %fname, i32* %M, i32* %N, i32* %nz, i32** nocapture %I, i32** nocapture %J, double** nocapture %val, [4 x i8]* nocapture %matcode) local_unnamed_addr #0 {
entry:
  %call110 = tail call i32 @strcmp(i8* %fname, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.25, i64 0, i64 0)) #12
  %cmp117 = icmp eq i32 %call110, 0
  br i1 %cmp117, label %if.then119, label %if.else

if.then119:                                       ; preds = %entry
  %0 = load %struct._IO_FILE*, %struct._IO_FILE** @stdin, align 8, !tbaa !5
  br label %if.end125

if.else:                                          ; preds = %entry
  %call120 = tail call %struct._IO_FILE* @fopen(i8* %fname, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str, i64 0, i64 0))
  %cmp121 = icmp eq %struct._IO_FILE* %call120, null
  br i1 %cmp121, label %cleanup, label %if.end125

if.end125:                                        ; preds = %if.else, %if.then119
  %f.0 = phi %struct._IO_FILE* [ %0, %if.then119 ], [ %call120, %if.else ]
  %call126 = tail call i32 @mm_read_banner(%struct._IO_FILE* %f.0, [4 x i8]* %matcode)
  %cmp127 = icmp eq i32 %call126, 0
  br i1 %cmp127, label %if.end130, label %cleanup

if.end130:                                        ; preds = %if.end125
  %arraydecay = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 0
  %call131 = tail call i32 @mm_is_valid(i8* %arraydecay)
  %tobool = icmp eq i32 %call131, 0
  br i1 %tobool, label %cleanup, label %land.lhs.true132

land.lhs.true132:                                 ; preds = %if.end130
  %arrayidx133 = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 1
  %1 = load i8, i8* %arrayidx133, align 1, !tbaa !2
  %cmp135 = icmp eq i8 %1, 67
  br i1 %cmp135, label %land.lhs.true137, label %cleanup

land.lhs.true137:                                 ; preds = %land.lhs.true132
  %2 = load i8, i8* %arraydecay, align 1, !tbaa !2
  %cmp140 = icmp eq i8 %2, 77
  br i1 %cmp140, label %if.end143, label %cleanup

if.end143:                                        ; preds = %land.lhs.true137
  %call144 = tail call i32 @mm_read_mtx_crd_size(%struct._IO_FILE* %f.0, i32* %M, i32* %N, i32* %nz)
  %cmp145 = icmp eq i32 %call144, 0
  br i1 %cmp145, label %if.end148, label %cleanup

if.end148:                                        ; preds = %if.end143
  %3 = load i32, i32* %nz, align 4, !tbaa !7
  %conv149 = sext i32 %3 to i64
  %mul = shl nsw i64 %conv149, 2
  %call150 = tail call noalias i8* @malloc(i64 %mul) #12
  %4 = bitcast i32** %I to i8**
  store i8* %call150, i8** %4, align 8, !tbaa !5
  %call153 = tail call noalias i8* @malloc(i64 %mul) #12
  %5 = bitcast i32** %J to i8**
  store i8* %call153, i8** %5, align 8, !tbaa !5
  store double* null, double** %val, align 8, !tbaa !5
  %arrayidx154 = getelementptr inbounds [4 x i8], [4 x i8]* %matcode, i64 0, i64 2
  %6 = load i8, i8* %arrayidx154, align 1, !tbaa !2
  switch i8 %6, label %if.end198 [
    i8 67, label %if.then158
    i8 82, label %if.then174
    i8 80, label %if.then189
  ]

if.then158:                                       ; preds = %if.end148
  %mul159 = shl nsw i32 %3, 1
  %conv160 = sext i32 %mul159 to i64
  %mul161 = shl nsw i64 %conv160, 3
  %call162 = tail call noalias i8* @malloc(i64 %mul161) #12
  %7 = bitcast i8* %call162 to double*
  %8 = bitcast double** %val to i8**
  store i8* %call162, i8** %8, align 8, !tbaa !5
  %9 = load i32*, i32** %I, align 8, !tbaa !5
  %10 = load i32*, i32** %J, align 8, !tbaa !5
  %call164 = tail call i32 @mm_read_mtx_crd_data(%struct._IO_FILE* %f.0, i32 undef, i32 undef, i32 %3, i32* %9, i32* %10, double* %7, i8* nonnull %arraydecay)
  %cmp165 = icmp eq i32 %call164, 0
  br i1 %cmp165, label %if.end198, label %cleanup

if.then174:                                       ; preds = %if.end148
  %mul176 = shl nsw i64 %conv149, 3
  %call177 = tail call noalias i8* @malloc(i64 %mul176) #12
  %11 = bitcast i8* %call177 to double*
  %12 = bitcast double** %val to i8**
  store i8* %call177, i8** %12, align 8, !tbaa !5
  %13 = load i32*, i32** %I, align 8, !tbaa !5
  %14 = load i32*, i32** %J, align 8, !tbaa !5
  %call179 = tail call i32 @mm_read_mtx_crd_data(%struct._IO_FILE* %f.0, i32 undef, i32 undef, i32 %3, i32* %13, i32* %14, double* %11, i8* nonnull %arraydecay)
  %cmp180 = icmp eq i32 %call179, 0
  br i1 %cmp180, label %if.end198, label %cleanup

if.then189:                                       ; preds = %if.end148
  %15 = load i32*, i32** %I, align 8, !tbaa !5
  %16 = load i32*, i32** %J, align 8, !tbaa !5
  %call191 = tail call i32 @mm_read_mtx_crd_data(%struct._IO_FILE* %f.0, i32 undef, i32 undef, i32 %3, i32* %15, i32* %16, double* null, i8* nonnull %arraydecay)
  %cmp192 = icmp eq i32 %call191, 0
  br i1 %cmp192, label %if.end198, label %cleanup

if.end198:                                        ; preds = %if.end148, %if.then158, %if.then174, %if.then189
  %17 = load %struct._IO_FILE*, %struct._IO_FILE** @stdin, align 8, !tbaa !5
  %cmp199 = icmp eq %struct._IO_FILE* %f.0, %17
  br i1 %cmp199, label %cleanup, label %if.then201

if.then201:                                       ; preds = %if.end198
  %call202 = tail call i32 @fclose(%struct._IO_FILE* %f.0)
  br label %cleanup

cleanup:                                          ; preds = %if.then201, %if.end198, %if.then189, %if.then174, %if.then158, %if.end143, %land.lhs.true132, %land.lhs.true137, %if.end130, %if.end125, %if.else
  %retval.0 = phi i32 [ 11, %if.else ], [ %call126, %if.end125 ], [ 15, %if.end130 ], [ 15, %land.lhs.true137 ], [ 15, %land.lhs.true132 ], [ %call144, %if.end143 ], [ %call164, %if.then158 ], [ %call179, %if.then174 ], [ %call191, %if.then189 ], [ 0, %if.end198 ], [ 0, %if.then201 ]
  ret i32 %retval.0
}

; Function Attrs: nounwind uwtable
define dso_local i32 @mm_write_banner(%struct._IO_FILE* nocapture %f, i8* nocapture readonly %matcode) local_unnamed_addr #0 {
entry:
  %call = tail call i8* @mm_typecode_to_str(i8* %matcode)
  %call1 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %f, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.26, i64 0, i64 0), i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str.8, i64 0, i64 0), i8* %call)
  tail call void @free(i8* %call) #12
  %cmp = icmp eq i32 %call1, 2
  %. = select i1 %cmp, i32 0, i32 17
  ret i32 %.
}

; Function Attrs: nounwind
declare dso_local void @free(i8* nocapture) local_unnamed_addr #9

; Function Attrs: nounwind uwtable
define dso_local i32 @mm_write_mtx_crd(i8* nocapture readonly %fname, i32 %M, i32 %N, i32 %nz, i32* nocapture readonly %I, i32* nocapture readonly %J, double* nocapture readonly %val, i8* nocapture readonly %matcode) local_unnamed_addr #0 {
entry:
  %call110 = tail call i32 @strcmp(i8* %fname, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.27, i64 0, i64 0)) #12
  %cmp117 = icmp eq i32 %call110, 0
  br i1 %cmp117, label %if.then119, label %if.else

if.then119:                                       ; preds = %entry
  %0 = load %struct._IO_FILE*, %struct._IO_FILE** @stdout, align 8, !tbaa !5
  br label %if.end125

if.else:                                          ; preds = %entry
  %call120 = tail call %struct._IO_FILE* @fopen(i8* %fname, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.28, i64 0, i64 0))
  %cmp121 = icmp eq %struct._IO_FILE* %call120, null
  br i1 %cmp121, label %cleanup, label %if.end125

if.end125:                                        ; preds = %if.else, %if.then119
  %f.0 = phi %struct._IO_FILE* [ %0, %if.then119 ], [ %call120, %if.else ]
  %call126 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %f.0, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.29, i64 0, i64 0), i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str.8, i64 0, i64 0))
  %call127 = tail call i8* @mm_typecode_to_str(i8* %matcode)
  %call128 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %f.0, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.30, i64 0, i64 0), i8* %call127)
  %call129 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %f.0, i8* getelementptr inbounds ([10 x i8], [10 x i8]* @.str.20, i64 0, i64 0), i32 %M, i32 %N, i32 %nz)
  %arrayidx130 = getelementptr inbounds i8, i8* %matcode, i64 2
  %1 = load i8, i8* %arrayidx130, align 1, !tbaa !2
  switch i8 %1, label %if.else184 [
    i8 80, label %for.cond.preheader
    i8 82, label %for.cond147.preheader
    i8 67, label %for.cond167.preheader
  ]

for.cond167.preheader:                            ; preds = %if.end125
  %cmp168259 = icmp sgt i32 %nz, 0
  br i1 %cmp168259, label %for.body170.preheader, label %if.end192

for.body170.preheader:                            ; preds = %for.cond167.preheader
  %wide.trip.count271 = zext i32 %nz to i64
  br label %for.body170

for.cond147.preheader:                            ; preds = %if.end125
  %cmp148257 = icmp sgt i32 %nz, 0
  br i1 %cmp148257, label %for.body150.preheader, label %if.end192

for.body150.preheader:                            ; preds = %for.cond147.preheader
  %wide.trip.count265 = zext i32 %nz to i64
  br label %for.body150

for.cond.preheader:                               ; preds = %if.end125
  %cmp135255 = icmp sgt i32 %nz, 0
  br i1 %cmp135255, label %for.body.preheader, label %if.end192

for.body.preheader:                               ; preds = %for.cond.preheader
  %wide.trip.count = zext i32 %nz to i64
  br label %for.body

for.body:                                         ; preds = %for.body, %for.body.preheader
  %indvars.iv = phi i64 [ 0, %for.body.preheader ], [ %indvars.iv.next, %for.body ]
  %arrayidx137 = getelementptr inbounds i32, i32* %I, i64 %indvars.iv
  %2 = load i32, i32* %arrayidx137, align 4, !tbaa !7
  %arrayidx139 = getelementptr inbounds i32, i32* %J, i64 %indvars.iv
  %3 = load i32, i32* %arrayidx139, align 4, !tbaa !7
  %call140 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %f.0, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.23, i64 0, i64 0), i32 %2, i32 %3)
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1
  %exitcond = icmp eq i64 %indvars.iv.next, %wide.trip.count
  br i1 %exitcond, label %if.end192, label %for.body

for.body150:                                      ; preds = %for.body150, %for.body150.preheader
  %indvars.iv263 = phi i64 [ 0, %for.body150.preheader ], [ %indvars.iv.next264, %for.body150 ]
  %arrayidx152 = getelementptr inbounds i32, i32* %I, i64 %indvars.iv263
  %4 = load i32, i32* %arrayidx152, align 4, !tbaa !7
  %arrayidx154 = getelementptr inbounds i32, i32* %J, i64 %indvars.iv263
  %5 = load i32, i32* %arrayidx154, align 4, !tbaa !7
  %arrayidx156 = getelementptr inbounds double, double* %val, i64 %indvars.iv263
  %6 = load double, double* %arrayidx156, align 8, !tbaa !9
  %call157 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %f.0, i8* getelementptr inbounds ([15 x i8], [15 x i8]* @.str.31, i64 0, i64 0), i32 %4, i32 %5, double %6)
  %indvars.iv.next264 = add nuw nsw i64 %indvars.iv263, 1
  %exitcond266 = icmp eq i64 %indvars.iv.next264, %wide.trip.count265
  br i1 %exitcond266, label %if.end192, label %for.body150

for.body170:                                      ; preds = %for.body170, %for.body170.preheader
  %indvars.iv267 = phi i64 [ 0, %for.body170.preheader ], [ %indvars.iv.next268, %for.body170 ]
  %arrayidx172 = getelementptr inbounds i32, i32* %I, i64 %indvars.iv267
  %7 = load i32, i32* %arrayidx172, align 4, !tbaa !7
  %arrayidx174 = getelementptr inbounds i32, i32* %J, i64 %indvars.iv267
  %8 = load i32, i32* %arrayidx174, align 4, !tbaa !7
  %9 = shl nuw nsw i64 %indvars.iv267, 1
  %arrayidx176 = getelementptr inbounds double, double* %val, i64 %9
  %10 = load double, double* %arrayidx176, align 8, !tbaa !9
  %11 = or i64 %9, 1
  %arrayidx179 = getelementptr inbounds double, double* %val, i64 %11
  %12 = load double, double* %arrayidx179, align 8, !tbaa !9
  %call180 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %f.0, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.32, i64 0, i64 0), i32 %7, i32 %8, double %10, double %12)
  %indvars.iv.next268 = add nuw nsw i64 %indvars.iv267, 1
  %exitcond272 = icmp eq i64 %indvars.iv.next268, %wide.trip.count271
  br i1 %exitcond272, label %if.end192, label %for.body170

if.else184:                                       ; preds = %if.end125
  %13 = load %struct._IO_FILE*, %struct._IO_FILE** @stdout, align 8, !tbaa !5
  %cmp185 = icmp eq %struct._IO_FILE* %f.0, %13
  br i1 %cmp185, label %cleanup, label %if.then187

if.then187:                                       ; preds = %if.else184
  %call188 = tail call i32 @fclose(%struct._IO_FILE* %f.0)
  br label %cleanup

if.end192:                                        ; preds = %for.body170, %for.body150, %for.body, %for.cond167.preheader, %for.cond147.preheader, %for.cond.preheader
  %14 = load %struct._IO_FILE*, %struct._IO_FILE** @stdout, align 8, !tbaa !5
  %cmp193 = icmp eq %struct._IO_FILE* %f.0, %14
  br i1 %cmp193, label %cleanup, label %if.then195

if.then195:                                       ; preds = %if.end192
  %call196 = tail call i32 @fclose(%struct._IO_FILE* %f.0)
  br label %cleanup

cleanup:                                          ; preds = %if.then195, %if.end192, %if.then187, %if.else184, %if.else
  %retval.0 = phi i32 [ 17, %if.else ], [ 15, %if.else184 ], [ 15, %if.then187 ], [ 0, %if.end192 ], [ 0, %if.then195 ]
  ret i32 %retval.0
}

; Function Attrs: nofree nounwind uwtable
define dso_local i8* @mm_strdup(i8* nocapture readonly %s) local_unnamed_addr #8 {
entry:
  %call = tail call i64 @strlen(i8* %s) #15
  %add = shl i64 %call, 32
  %sext = add i64 %add, 4294967296
  %conv1 = ashr exact i64 %sext, 32
  %call2 = tail call noalias i8* @malloc(i64 %conv1) #12
  %call3 = tail call i8* @strcpy(i8* %call2, i8* %s) #12
  ret i8* %call3
}

; Function Attrs: nofree nounwind
declare dso_local i8* @strcpy(i8* returned, i8* nocapture readonly) local_unnamed_addr #2

; Function Attrs: nofree nounwind
declare dso_local i32 @sprintf(i8* nocapture, i8* nocapture readonly, ...) local_unnamed_addr #2

; Function Attrs: nofree nounwind
declare i64 @fwrite(i8* nocapture, i64, i64, %struct._IO_FILE* nocapture) local_unnamed_addr #10

; Function Attrs: nofree nounwind readonly
declare i32 @bcmp(i8* nocapture, i8* nocapture, i64) local_unnamed_addr #11

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { nofree nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #3 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #4 = { norecurse nounwind readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #5 = { nounwind readnone "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #6 = { argmemonly nofree nounwind readonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #7 = { nofree nounwind readonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #8 = { nofree nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #9 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #10 = { nofree nounwind }
attributes #11 = { nofree nounwind readonly }
attributes #12 = { nounwind }
attributes #13 = { cold }
attributes #14 = { nounwind readnone }
attributes #15 = { nounwind readonly }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (https://gitlab.engr.illinois.edu/llvm/hpvm.git e24d6a1d89c07757edeea087e4ccf2ac27fe9fc7)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"omnipotent char", !4, i64 0}
!4 = !{!"Simple C/C++ TBAA"}
!5 = !{!6, !6, i64 0}
!6 = !{!"any pointer", !3, i64 0}
!7 = !{!8, !8, i64 0}
!8 = !{!"int", !3, i64 0}
!9 = !{!10, !10, i64 0}
!10 = !{!"double", !3, i64 0}

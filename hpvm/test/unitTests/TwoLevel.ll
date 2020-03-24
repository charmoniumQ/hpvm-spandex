; RUN: opt -load LLVMGenHPVM.so -S -genhpvm < %s
; ModuleID = 'TwoLevel.c'
source_filename = "TwoLevel.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Root = type { i32*, i64, i32*, i64, i32*, i64 }

; Function Attrs: nounwind uwtable
define dso_local void @Func1(i32* %In, i64 %Insize, i32* %Out, i64 %Outsize) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #3
  tail call void (i32, ...) @__hpvm__attributes(i32 1, i32* %In, i32* %Out, i32 1, i32* %Out) #3
  %0 = load i32, i32* %In, align 4, !tbaa !2
  store i32 %0, i32* %Out, align 4, !tbaa !2
  tail call void (i32, ...) @__hpvm__return(i32 1, i32* %Out) #3
  ret void
}

declare dso_local void @__hpvm__hint(i32) local_unnamed_addr #1

declare dso_local void @__hpvm__attributes(i32, ...) local_unnamed_addr #1

declare dso_local void @__hpvm__return(i32, ...) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define dso_local void @Func2(i32* %In, i64 %Insize, i32* %Out, i64 %Outsize) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #3
  tail call void (i32, ...) @__hpvm__attributes(i32 2, i32* %In, i32* %Out, i32 1, i32* %Out) #3
  %call = tail call i8* (i32, ...) @__hpvm__createNodeND(i32 1, void (i32*, i64, i32*, i64)* nonnull @Func1, i64 3) #3
  tail call void @__hpvm__bindIn(i8* %call, i32 0, i32 0, i32 0) #3
  tail call void @__hpvm__bindIn(i8* %call, i32 1, i32 1, i32 0) #3
  tail call void @__hpvm__bindIn(i8* %call, i32 2, i32 2, i32 0) #3
  tail call void @__hpvm__bindIn(i8* %call, i32 3, i32 3, i32 0) #3
  tail call void @__hpvm__bindOut(i8* %call, i32 0, i32 0, i32 0) #3
  ret void
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

declare dso_local i8* @__hpvm__createNodeND(i32, ...) local_unnamed_addr #1

declare dso_local void @__hpvm__bindIn(i8*, i32, i32, i32) local_unnamed_addr #1

declare dso_local void @__hpvm__bindOut(i8*, i32, i32, i32) local_unnamed_addr #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: nounwind uwtable
define dso_local void @PipeRoot(i32* %In1, i64 %Insize1, i32* %In2, i64 %InSize2, i32* %Out, i64 %Outsize) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #3
  tail call void (i32, ...) @__hpvm__attributes(i32 3, i32* %In1, i32* %In2, i32* %Out, i32 1, i32* %Out) #3
  %call = tail call i8* (i32, ...) @__hpvm__createNodeND(i32 0, void (i32*, i64, i32*, i64)* nonnull @Func2) #3
  tail call void @__hpvm__bindIn(i8* %call, i32 0, i32 0, i32 0) #3
  tail call void @__hpvm__bindIn(i8* %call, i32 1, i32 1, i32 0) #3
  tail call void @__hpvm__bindIn(i8* %call, i32 2, i32 2, i32 0) #3
  tail call void @__hpvm__bindIn(i8* %call, i32 3, i32 3, i32 0) #3
  tail call void @__hpvm__bindOut(i8* %call, i32 0, i32 0, i32 0) #3
  ret void
}

; Function Attrs: nounwind uwtable
define dso_local i32 @main() local_unnamed_addr #0 {
entry:
  %In1 = alloca i32, align 4
  %In2 = alloca i32, align 4
  %Out = alloca i32, align 4
  %RootArgs = alloca %struct.Root, align 8
  %0 = bitcast i32* %In1 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #3
  store i32 1, i32* %In1, align 4, !tbaa !2
  %1 = bitcast i32* %In2 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %1) #3
  store i32 2, i32* %In2, align 4, !tbaa !2
  %2 = bitcast i32* %Out to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %2) #3
  store i32 0, i32* %Out, align 4, !tbaa !2
  %3 = bitcast %struct.Root* %RootArgs to i8*
  call void @llvm.lifetime.start.p0i8(i64 48, i8* nonnull %3) #3
  %input1 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 0
  store i32* %In1, i32** %input1, align 8, !tbaa !6
  %Insize1 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 1
  store i64 32, i64* %Insize1, align 8, !tbaa !10
  %input2 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 2
  store i32* %In2, i32** %input2, align 8, !tbaa !11
  %Insize2 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 3
  store i64 32, i64* %Insize2, align 8, !tbaa !12
  %output = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 4
  store i32* %Out, i32** %output, align 8, !tbaa !13
  %Outsize = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 5
  store i64 32, i64* %Outsize, align 8, !tbaa !14
  call void (...) @__hpvm__init() #3
  %call = call i8* (i32, ...) @__hpvm__launch(i32 0, void (i32*, i64, i32*, i64, i32*, i64)* nonnull @PipeRoot, %struct.Root* nonnull %RootArgs) #3
  call void @__hpvm__wait(i8* %call) #3
  call void (...) @__hpvm__cleanup() #3
  call void @llvm.lifetime.end.p0i8(i64 48, i8* nonnull %3) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %2) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %1) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #3
  ret i32 0
}

declare dso_local void @__hpvm__init(...) local_unnamed_addr #1

declare dso_local i8* @__hpvm__launch(i32, ...) local_unnamed_addr #1

declare dso_local void @__hpvm__wait(i8*) local_unnamed_addr #1

declare dso_local void @__hpvm__cleanup(...) local_unnamed_addr #1

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #1 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (https://gitlab.engr.illinois.edu/llvm/hpvm.git 3551132592a00cab6c966df508ab511598269f78)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"int", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
!6 = !{!7, !8, i64 0}
!7 = !{!"Root", !8, i64 0, !9, i64 8, !8, i64 16, !9, i64 24, !8, i64 32, !9, i64 40}
!8 = !{!"any pointer", !4, i64 0}
!9 = !{!"long", !4, i64 0}
!10 = !{!7, !9, i64 8}
!11 = !{!7, !8, i64 16}
!12 = !{!7, !9, i64 24}
!13 = !{!7, !8, i64 32}
!14 = !{!7, !9, i64 40}

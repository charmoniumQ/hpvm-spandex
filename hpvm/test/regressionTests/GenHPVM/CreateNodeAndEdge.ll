; RUN: opt -load LLVMGenHPVM.so -S -genhpvm <  %s | FileCheck %s
; ModuleID = 'CreateNodeAndEdge.c'
source_filename = "CreateNodeAndEdge.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Root = type { i32*, i32*, i32* }
; CHECK-LABEL: struct.Root =

; CHECK-LABEL: %struct.out.Func1 =
; CHECK-LABEL: %struct.out.Func2 =
; CHECK-LABEL: %struct.out.PipeRoot =

; CHECK-LABEL: i32 @main(
; CHECK: [[ALLOCA:%[1-9a-zA-Z]+]] = alloca %struct.Root
; CHECK: call void @llvm.hpvm.init()
; CHECK:  [[REGISTER:%[1-9]+]] = bitcast %struct.Root* [[ALLOCA]] to i8*
; CHECK: call i8* @llvm.hpvm.launch(i8* bitcast (%struct.out.PipeRoot (i32*, i32*, i32*)* @PipeRoot_cloned to i8*), i8* [[REGISTER]],
; CHECK-NEXT: call void @llvm.hpvm.wait(i8*

; CHECK-LABEL: @Func1_cloned(
; CHECK: [[RET1:%[1-9a-zA-Z]+]] = insertvalue %struct.out.Func1 undef,
; CHECK-NEXT: ret %struct.out.Func1 [[RET1]]

; CHECK-LABEL: @Func2_cloned(
; CHECK: [[RET2:%[1-9a-zA-Z]+]] = insertvalue %struct.out.Func2 undef,
; CHECK-NEXT: ret %struct.out.Func2 [[RET2]]


; CHECK-LABEL: @PipeRoot_cloned(i32*
; CHECK: %Func1_cloned.node = call i8* @llvm.hpvm.createNode(i8* bitcast (%struct.out.Func1 (i32*
; CHECK: %Func2_cloned.node = call i8* @llvm.hpvm.createNode(i8* bitcast (%struct.out.Func2 (i32*
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func1_cloned.node,
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node,
; CHECK-NEXT: call i8* @llvm.hpvm.createEdge(i8* %Func1_cloned.node, i8* %Func2_cloned.node,
; CHECK-NEXT: @llvm.hpvm.bind.output(i8* %Func2_cloned.node



; Function Attrs: nounwind uwtable
define dso_local void @Func1(i32* %In, i32* %Out) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #3
  tail call void (i32, ...) @__hpvm__attributes(i32 1, i32* %In, i32 1, i32* %Out) #3
  tail call void (i32, ...) @__hpvm__return(i32 1, i32* %Out) #3
  ret void
}

declare dso_local void @__hpvm__hint(i32) local_unnamed_addr #1

declare dso_local void @__hpvm__attributes(i32, ...) local_unnamed_addr #1

declare dso_local void @__hpvm__return(i32, ...) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define dso_local void @Func2(i32* %BindIn, i32* %SrcIn, i32* %Out) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #3
  tail call void (i32, ...) @__hpvm__attributes(i32 2, i32* %BindIn, i32* %SrcIn, i32 1, i32* %Out) #3
  tail call void (i32, ...) @__hpvm__return(i32 1, i32* %Out) #3
  ret void
}

; Function Attrs: nounwind uwtable
define dso_local void @PipeRoot(i32* %In1, i32* %In2, i32* %Out) #0 {
entry:
  tail call void @__hpvm__hint(i32 1) #3
  tail call void (i32, ...) @__hpvm__attributes(i32 2, i32* %In1, i32* %In2, i32 1, i32* %Out) #3
  %call = tail call i8* (i32, ...) @__hpvm__createNodeND(i32 0, void (i32*, i32*)* nonnull @Func1) #3
  %call1 = tail call i8* (i32, ...) @__hpvm__createNodeND(i32 0, void (i32*, i32*, i32*)* nonnull @Func2) #3
  tail call void @__hpvm__bindIn(i8* %call, i32 0, i32 0, i32 0) #3
  tail call void @__hpvm__bindIn(i8* %call1, i32 1, i32 0, i32 0) #3
  %call2 = tail call i8* @__hpvm__edge(i8* %call, i8* %call1, i32 0, i32 1, i32 1, i32 0) #3
  tail call void @__hpvm__bindOut(i8* %call1, i32 0, i32 0, i32 0) #3
  ret void
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

declare dso_local i8* @__hpvm__createNodeND(i32, ...) local_unnamed_addr #1

declare dso_local void @__hpvm__bindIn(i8*, i32, i32, i32) local_unnamed_addr #1

declare dso_local i8* @__hpvm__edge(i8*, i8*, i32, i32, i32, i32) local_unnamed_addr #1

declare dso_local void @__hpvm__bindOut(i8*, i32, i32, i32) local_unnamed_addr #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

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
  call void @llvm.lifetime.start.p0i8(i64 24, i8* nonnull %3) #3
  %input1 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 0
  store i32* %In1, i32** %input1, align 8, !tbaa !6
  %input2 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 1
  store i32* %In2, i32** %input2, align 8, !tbaa !9
  %output = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 2
  store i32* %Out, i32** %output, align 8, !tbaa !10
  call void (...) @__hpvm__init() #3
  %call = call i8* (i32, ...) @__hpvm__launch(i32 0, void (i32*, i32*, i32*)* nonnull @PipeRoot, %struct.Root* nonnull %RootArgs) #3
  call void @__hpvm__wait(i8* %call) #3
  call void (...) @__hpvm__cleanup() #3
  call void @llvm.lifetime.end.p0i8(i64 24, i8* nonnull %3) #3
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
!1 = !{!"clang version 9.0.0 (https://gitlab.engr.illinois.edu/llvm/hpvm.git 6690f9e7e8b46b96aea222d3e85315cd63545953)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"int", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
!6 = !{!7, !8, i64 0}
!7 = !{!"Root", !8, i64 0, !8, i64 8, !8, i64 16}
!8 = !{!"any pointer", !4, i64 0}
!9 = !{!7, !8, i64 8}
!10 = !{!7, !8, i64 16}

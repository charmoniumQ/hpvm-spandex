; RUN: opt -load LLVMBuildDFG.so -S < %s | FileCheck %s
; ModuleID = 'LeafBindEdge.ll'
source_filename = "LeafBindEdge.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Root = type { i32*, i32*, i32* }
%struct.out.Func1 = type <{ i32* }>
%struct.out.Func4 = type <{ i32* }>
%struct.out.Func5 = type <{ i32* }>
%struct.out.Func3 = type <{ i32* }>
%struct.out.Func2 = type <{ i32* }>
%struct.out.PipeRoot = type <{ i32* }>

; CHECK-LABEL: struct.Root =

; CHECK-LABEL: %struct.out.Func1 =
; CHECK-LABEL: %struct.out.Func4 =
; CHECK-LABEL: %struct.out.Func5 =
; CHECK-LABEL: %struct.out.Func3 =
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

; CHECK-LABEL: @Func4_cloned(
; CHECK: [[RET4:%[1-9a-zA-Z]+]] = insertvalue %struct.out.Func4 undef,
; CHECK-NEXT: ret %struct.out.Func4 [[RET4]]

; CHECK-LABEL: @Func5_cloned(
; CHECK: [[RET5:%[1-9a-zA-Z]+]] = insertvalue %struct.out.Func5 undef,
; CHECK-NEXT: ret %struct.out.Func5 [[RET5]]

; CHECK-LABEL: @Func3_cloned(
; CHECK: %Func4_cloned.node = call i8* @llvm.hpvm.createNode2D(i8* bitcast (%struct.out.Func4 (i32*
; CHECK-NEXT: %Func5_cloned.node = call i8* @llvm.hpvm.createNode2D(i8* bitcast (%struct.out.Func5 (i32*
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func4_cloned.node,
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func5_cloned.node,
; CHECK-NEXT: call i8* @llvm.hpvm.createEdge(i8* %Func4_cloned.node, i8* %Func5_cloned.node,
; CHECK-NEXT: call void @llvm.hpvm.bind.output(i8* %Func5_cloned.node,

; CHECK-LABEL: @Func2_cloned(
; CHECK: %Func3_cloned.node = call i8* @llvm.hpvm.createNode1D(i8* bitcast (%struct.out.Func3 (i32*
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node,
; CHECK-NEXT: call void @llvm.hpvm.bind.output(i8* %Func3_cloned.node,

; CHECK-LABEL: @PipeRoot_cloned(i32*
; CHECK: %Func1_cloned.node = call i8* @llvm.hpvm.createNode(i8* bitcast (%struct.out.Func1 (i32*
; CHECK-NEXT: %Func2_cloned.node = call i8* @llvm.hpvm.createNode(i8* bitcast (%struct.out.Func2 (i32*
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func1_cloned.node,
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node,
; CHECK-NEXT: call i8* @llvm.hpvm.createEdge(i8* %Func1_cloned.node, i8* %Func2_cloned.node,
; CHECK-NEXT: call void @llvm.hpvm.bind.output(i8* %Func2_cloned.node,


declare dso_local void @__hpvm__hint(i32) local_unnamed_addr #0

declare dso_local void @__hpvm__attributes(i32, ...) local_unnamed_addr #0

declare dso_local void @__hpvm__return(i32, ...) local_unnamed_addr #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

declare dso_local i8* @__hpvm__createNodeND(i32, ...) local_unnamed_addr #0

declare dso_local void @__hpvm__bindIn(i8*, i32, i32, i32) local_unnamed_addr #0

declare dso_local i8* @__hpvm__edge(i8*, i8*, i32, i32, i32, i32) local_unnamed_addr #0

declare dso_local void @__hpvm__bindOut(i8*, i32, i32, i32) local_unnamed_addr #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nounwind uwtable
define dso_local i32 @main() local_unnamed_addr #2 {
entry:
  %In1 = alloca i32, align 4
  %In2 = alloca i32, align 4
  %Out = alloca i32, align 4
  %RootArgs = alloca %struct.Root, align 8
  %0 = bitcast i32* %In1 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #3
  store i32 1, i32* %In1, align 4, !tbaa !8
  %1 = bitcast i32* %In2 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %1) #3
  store i32 2, i32* %In2, align 4, !tbaa !8
  %2 = bitcast i32* %Out to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %2) #3
  store i32 0, i32* %Out, align 4, !tbaa !8
  %3 = bitcast %struct.Root* %RootArgs to i8*
  call void @llvm.lifetime.start.p0i8(i64 24, i8* nonnull %3) #3
  %input1 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 0
  store i32* %In1, i32** %input1, align 8, !tbaa !12
  %input2 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 1
  store i32* %In2, i32** %input2, align 8, !tbaa !15
  %output = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 2
  store i32* %Out, i32** %output, align 8, !tbaa !16
  call void @llvm.hpvm.init()
  %4 = bitcast %struct.Root* %RootArgs to i8*
  %graphID = call i8* @llvm.hpvm.launch(i8* bitcast (%struct.out.PipeRoot (i32*, i32*, i32*)* @PipeRoot_cloned to i8*), i8* %4, i1 false)
  call void @llvm.hpvm.wait(i8* %graphID)
  call void @llvm.hpvm.cleanup()
  call void @llvm.lifetime.end.p0i8(i64 24, i8* nonnull %3) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %2) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %1) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #3
  ret i32 0
}

declare dso_local void @__hpvm__init(...) local_unnamed_addr #0

declare dso_local i8* @__hpvm__launch(i32, ...) local_unnamed_addr #0

declare dso_local void @__hpvm__wait(i8*) local_unnamed_addr #0

declare dso_local void @__hpvm__cleanup(...) local_unnamed_addr #0

declare i8* @llvm_hpvm_initializeTimerSet()

declare void @llvm_hpvm_switchToTimer(i8**, i32)

declare void @llvm_hpvm_printTimerSet(i8**, i8*)

; Function Attrs: nounwind uwtable
define dso_local %struct.out.Func1 @Func1_cloned(i32* in %In, i32* out %Out) #2 {
entry:
  %returnStruct = insertvalue %struct.out.Func1 undef, i32* %Out, 0
  ret %struct.out.Func1 %returnStruct
}

; Function Attrs: nounwind uwtable
define dso_local %struct.out.Func4 @Func4_cloned(i32* in %In, i32* out %Out) #2 {
entry:
  %returnStruct = insertvalue %struct.out.Func4 undef, i32* %Out, 0
  ret %struct.out.Func4 %returnStruct
}

; Function Attrs: nounwind uwtable
define dso_local %struct.out.Func5 @Func5_cloned(i32* in %In1, i32* in %In2, i32* out %Out) #2 {
entry:
  %returnStruct = insertvalue %struct.out.Func5 undef, i32* %Out, 0
  ret %struct.out.Func5 %returnStruct
}

; Function Attrs: nounwind
declare i8* @llvm.hpvm.createNode2D(i8*, i64, i64) #3

; Function Attrs: nounwind
declare void @llvm.hpvm.bind.input(i8*, i32, i32, i1) #3

; Function Attrs: nounwind
declare i8* @llvm.hpvm.createEdge(i8*, i8*, i1, i32, i32, i1) #3

; Function Attrs: nounwind
declare void @llvm.hpvm.bind.output(i8*, i32, i32, i1) #3

; Function Attrs: nounwind uwtable
define dso_local %struct.out.Func3 @Func3_cloned(i32* in %In, i32* out %Out) #2 {
entry:
  %Func4_cloned.node = call i8* @llvm.hpvm.createNode2D(i8* bitcast (%struct.out.Func4 (i32*, i32*)* @Func4_cloned to i8*), i64 3, i64 6)
  %Func5_cloned.node = call i8* @llvm.hpvm.createNode2D(i8* bitcast (%struct.out.Func5 (i32*, i32*, i32*)* @Func5_cloned to i8*), i64 4, i64 5)
  call void @llvm.hpvm.bind.input(i8* %Func4_cloned.node, i32 0, i32 0, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func5_cloned.node, i32 1, i32 0, i1 false)
  %output = call i8* @llvm.hpvm.createEdge(i8* %Func4_cloned.node, i8* %Func5_cloned.node, i1 false, i32 1, i32 1, i1 false)
  call void @llvm.hpvm.bind.output(i8* %Func5_cloned.node, i32 0, i32 0, i1 false)
  ret %struct.out.Func3 undef
}

; Function Attrs: nounwind
declare i8* @llvm.hpvm.createNode1D(i8*, i64) #3

; Function Attrs: nounwind uwtable
define dso_local %struct.out.Func2 @Func2_cloned(i32* in %BindIn, i32* in %SrcIn, i32* out %Out) #2 {
entry:
  %Func3_cloned.node = call i8* @llvm.hpvm.createNode1D(i8* bitcast (%struct.out.Func3 (i32*, i32*)* @Func3_cloned to i8*), i64 3)
  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node, i32 0, i32 0, i1 false)
  call void @llvm.hpvm.bind.output(i8* %Func3_cloned.node, i32 0, i32 0, i1 false)
  ret %struct.out.Func2 undef
}

; Function Attrs: nounwind
declare i8* @llvm.hpvm.createNode(i8*) #3

; Function Attrs: nounwind uwtable
define dso_local %struct.out.PipeRoot @PipeRoot_cloned(i32* in %In1, i32* in %In2, i32* out %Out) #2 {
entry:
  %Func1_cloned.node = call i8* @llvm.hpvm.createNode(i8* bitcast (%struct.out.Func1 (i32*, i32*)* @Func1_cloned to i8*))
  %Func2_cloned.node = call i8* @llvm.hpvm.createNode(i8* bitcast (%struct.out.Func2 (i32*, i32*, i32*)* @Func2_cloned to i8*))
  call void @llvm.hpvm.bind.input(i8* %Func1_cloned.node, i32 0, i32 0, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node, i32 1, i32 0, i1 false)
  %output = call i8* @llvm.hpvm.createEdge(i8* %Func1_cloned.node, i8* %Func2_cloned.node, i1 false, i32 1, i32 1, i1 false)
  call void @llvm.hpvm.bind.output(i8* %Func2_cloned.node, i32 0, i32 0, i1 false)
  ret %struct.out.PipeRoot undef
}

; Function Attrs: nounwind
declare void @llvm.hpvm.init() #3

; Function Attrs: nounwind
declare i8* @llvm.hpvm.launch(i8*, i8*, i1) #3

; Function Attrs: nounwind
declare void @llvm.hpvm.wait(i8*) #3

; Function Attrs: nounwind
declare void @llvm.hpvm.cleanup() #3

attributes #0 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}
!hpvm_hint_cpu = !{!2, !3, !4, !5, !6, !7}
!hpvm_hint_gpu = !{}
!hpvm_hint_spir = !{}
!hpvm_hint_cudnn = !{}
!hpvm_hint_promise = !{}
!hpvm_hint_cpu_gpu = !{}
!hpvm_hint_cpu_spir = !{}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (https://gitlab.engr.illinois.edu/llvm/hpvm.git 6690f9e7e8b46b96aea222d3e85315cd63545953)"}
!2 = !{%struct.out.Func1 (i32*, i32*)* @Func1_cloned}
!3 = !{%struct.out.Func4 (i32*, i32*)* @Func4_cloned}
!4 = !{%struct.out.Func5 (i32*, i32*, i32*)* @Func5_cloned}
!5 = !{%struct.out.Func3 (i32*, i32*)* @Func3_cloned}
!6 = !{%struct.out.Func2 (i32*, i32*, i32*)* @Func2_cloned}
!7 = !{%struct.out.PipeRoot (i32*, i32*, i32*)* @PipeRoot_cloned}
!8 = !{!9, !9, i64 0}
!9 = !{!"int", !10, i64 0}
!10 = !{!"omnipotent char", !11, i64 0}
!11 = !{!"Simple C/C++ TBAA"}
!12 = !{!13, !14, i64 0}
!13 = !{!"Root", !14, i64 0, !14, i64 8, !14, i64 16}
!14 = !{!"any pointer", !10, i64 0}
!15 = !{!13, !14, i64 8}
!16 = !{!13, !14, i64 16}

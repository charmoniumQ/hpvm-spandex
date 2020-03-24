; RUN: opt -load LLVMBuildDFG.so -load LLVMDFG2LLVM_CPU.so -S -dfg2llvm-cpu <  %s | FileCheck %s
; ModuleID = 'CreateNode.ll'
source_filename = "CreateNode.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Root = type { i32*, i64, i32*, i64, i32*, i64 }
%struct.out.Func = type <{ i32* }>
%struct.out.PipeRoot = type <{ i32* }>

; CHECK-LABEL: i32 @main(
; CHECK: call void @llvm.hpvm.init()
; CHECK: call i8* @llvm_hpvm_cpu_launch(i8* (i8*)* @LaunchDataflowGraph, i8*
; CHECK-NEXT: call i8* @llvm.hpvm.launch(i8*
; CHECK-NEXT: call void @llvm_hpvm_cpu_wait(i8*

; CHECK-LABEL: @PipeRoot_cloned(
; CHECK: call i8* @llvm.hpvm.createNode(
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.output(i8* %Func_cloned.node

; CHECK-LABEL: @Func_cloned.1_cloned_cloned_cloned_cloned_cloned_cloned
; CHECK: call i8* @llvm_hpvm_cpu_argument_ptr(

; CHECK-LABEL: @PipeRoot_cloned.2(
; CHECK: call void @llvm_hpvm_cpu_dstack_push(
; CHECK-NEXT: @Func_cloned.1_cloned_cloned_cloned_cloned_cloned_cloned(
; CHECK-NEXT: call void @llvm_hpvm_cpu_dstack_pop()

; CHECK-LABEL: @LaunchDataflowGraph(i8*
; call %struct.out.PipeRoot @PipeRoot_cloned.2(


declare dso_local void @__hpvm__hint(i32) local_unnamed_addr #0

declare dso_local void @__hpvm__attributes(i32, ...) local_unnamed_addr #0

declare dso_local void @__hpvm__return(i32, ...) local_unnamed_addr #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

declare dso_local i8* @__hpvm__createNodeND(i32, ...) local_unnamed_addr #0

declare dso_local void @__hpvm__bindIn(i8*, i32, i32, i32) local_unnamed_addr #0

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
  store i32 1, i32* %In1, align 4, !tbaa !4
  %1 = bitcast i32* %In2 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %1) #3
  store i32 2, i32* %In2, align 4, !tbaa !4
  %2 = bitcast i32* %Out to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %2) #3
  store i32 0, i32* %Out, align 4, !tbaa !4
  %3 = bitcast %struct.Root* %RootArgs to i8*
  call void @llvm.lifetime.start.p0i8(i64 48, i8* nonnull %3) #3
  %input1 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 0
  store i32* %In1, i32** %input1, align 8, !tbaa !8
  %Insize1 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 1
  store i64 32, i64* %Insize1, align 8, !tbaa !12
  %input2 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 2
  store i32* %In2, i32** %input2, align 8, !tbaa !13
  %Insize2 = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 3
  store i64 32, i64* %Insize2, align 8, !tbaa !14
  %output = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 4
  store i32* %Out, i32** %output, align 8, !tbaa !15
  %Outsize = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 5
  store i64 32, i64* %Outsize, align 8, !tbaa !16
  call void @llvm.hpvm.init()
  %4 = bitcast %struct.Root* %RootArgs to i8*
  %graphID = call i8* @llvm.hpvm.launch(i8* bitcast (%struct.out.PipeRoot (i32*, i64, i32*, i64, i32*, i64)* @PipeRoot_cloned to i8*), i8* %4, i1 false)
  call void @llvm.hpvm.wait(i8* %graphID)
  call void @llvm.hpvm.cleanup()
  call void @llvm.lifetime.end.p0i8(i64 48, i8* nonnull %3) #3
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
define dso_local %struct.out.Func @Func_cloned(i32* in %In, i64 %Insize, i32* out %Out, i64 %Outsize) #2 {
entry:
  %returnStruct = insertvalue %struct.out.Func undef, i32* %Out, 0
  ret %struct.out.Func %returnStruct
}

; Function Attrs: nounwind
declare i8* @llvm.hpvm.createNode(i8*) #3

; Function Attrs: nounwind
declare void @llvm.hpvm.bind.input(i8*, i32, i32, i1) #3

; Function Attrs: nounwind
declare void @llvm.hpvm.bind.output(i8*, i32, i32, i1) #3

; Function Attrs: nounwind uwtable
define dso_local %struct.out.PipeRoot @PipeRoot_cloned(i32* in %In1, i64 %Insize1, i32* in %In2, i64 %InSize2, i32* out %Out, i64 %Outsize) #2 {
entry:
  %Func_cloned.node = call i8* @llvm.hpvm.createNode(i8* bitcast (%struct.out.Func (i32*, i64, i32*, i64)* @Func_cloned to i8*))
  call void @llvm.hpvm.bind.input(i8* %Func_cloned.node, i32 0, i32 0, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func_cloned.node, i32 1, i32 1, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func_cloned.node, i32 2, i32 2, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func_cloned.node, i32 3, i32 3, i1 false)
  call void @llvm.hpvm.bind.output(i8* %Func_cloned.node, i32 0, i32 0, i1 false)
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

attributes #0 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="cpu-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="cpu-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}
!hpvm_hint_cpu = !{!2, !3}
!hpvm_hint_gpu = !{}
!hpvm_hint_spir = !{}
!hpvm_hint_cudnn = !{}
!hpvm_hint_promise = !{}
!hpvm_hint_cpu_gpu = !{}
!hpvm_hint_cpu_spir = !{}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (https://gitlab.engr.illinois.edu/llvm/hpvm.git 6690f9e7e8b46b96aea222d3e85315cd63545953)"}
!2 = !{%struct.out.Func (i32*, i64, i32*, i64)* @Func_cloned}
!3 = !{%struct.out.PipeRoot (i32*, i64, i32*, i64, i32*, i64)* @PipeRoot_cloned}
!4 = !{!5, !5, i64 0}
!5 = !{!"int", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C/C++ TBAA"}
!8 = !{!9, !10, i64 0}
!9 = !{!"Root", !10, i64 0, !11, i64 8, !10, i64 16, !11, i64 24, !10, i64 32, !11, i64 40}
!10 = !{!"any pointer", !6, i64 0}
!11 = !{!"long", !6, i64 0}
!12 = !{!9, !11, i64 8}
!13 = !{!9, !10, i64 16}
!14 = !{!9, !11, i64 24}
!15 = !{!9, !10, i64 32}
!16 = !{!9, !11, i64 40}

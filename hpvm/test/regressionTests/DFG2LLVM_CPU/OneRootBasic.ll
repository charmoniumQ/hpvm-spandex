; RUN: opt -load LLVMBuildDFG.so -load LLVMDFG2LLVM_CPU.so -S -dfg2llvm-cpu <  %s | FileCheck %s
; ModuleID = 'oneLaunchAlloca.ll'
source_filename = "oneLaunchAlloca.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Root = type { i32*, i64, i32*, i64 }
%emptyStruct = type <{}>

declare dso_local void @__hpvm__hint(i32) local_unnamed_addr #0

declare dso_local void @__hpvm__attributes(i32, ...) local_unnamed_addr #0

; CHECK-LABEL: i32 @main(
; CHECK: call void @llvm.hpvm.init()
; CHECK: call i8* @llvm_hpvm_cpu_launch(i8* (i8*)* @LaunchDataflowGraph, i8*
; CHECK-NEXT: call i8* @llvm.hpvm.launch(i8*
; CHECK-NEXT: call void @llvm_hpvm_cpu_wait(i8*

; CHECK-LABEL: @PipeRoot_cloned.1(

; CHECK-LABEL: @LaunchDataflowGraph(i8*
; call %struct.out.PipeRoot @PipeRoot_cloned.1(


; Function Attrs: nounwind uwtable
define dso_local i32 @main() local_unnamed_addr #1 {
entry:
  %In = alloca i32, align 4
  %Out = alloca i32, align 4
  %RootArgs = alloca %struct.Root, align 8
  %0 = bitcast i32* %In to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #3
  %1 = bitcast i32* %Out to i8*
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %1) #3
  %2 = bitcast %struct.Root* %RootArgs to i8*
  call void @llvm.lifetime.start.p0i8(i64 32, i8* nonnull %2) #3
  %input = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 0
  store i32* %In, i32** %input, align 8, !tbaa !3
  %insize = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 1
  store i64 32, i64* %insize, align 8, !tbaa !9
  %output = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 2
  store i32* %Out, i32** %output, align 8, !tbaa !10
  %outsize = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 3
  store i64 32, i64* %outsize, align 8, !tbaa !11
  call void @llvm.hpvm.init()
  %3 = bitcast %struct.Root* %RootArgs to i8*
  %graphID = call i8* @llvm.hpvm.launch(i8* bitcast (%emptyStruct (i32*, i64, i32*, i64)* @PipeRoot_cloned to i8*), i8* %3, i1 false)
  call void @llvm.hpvm.wait(i8* %graphID)
  call void @llvm.hpvm.cleanup()
  call void @llvm.lifetime.end.p0i8(i64 32, i8* nonnull %2) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %1) #3
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #3
  ret i32 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

declare dso_local void @__hpvm__init(...) local_unnamed_addr #0

declare dso_local i8* @__hpvm__launch(i32, ...) local_unnamed_addr #0

declare dso_local void @__hpvm__wait(i8*) local_unnamed_addr #0

declare dso_local void @__hpvm__cleanup(...) local_unnamed_addr #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

declare i8* @llvm_hpvm_initializeTimerSet()

declare void @llvm_hpvm_switchToTimer(i8**, i32)

declare void @llvm_hpvm_printTimerSet(i8**, i8*)

; Function Attrs: nounwind
declare void @llvm.hpvm.init() #3

; Function Attrs: nounwind
declare i8* @llvm.hpvm.launch(i8*, i8*, i1) #3

; Function Attrs: nounwind uwtable
define dso_local %emptyStruct @PipeRoot_cloned(i32* in %In, i64 %insize, i32* out %Out, i64 %outsize) #1 {
entry:
  ret %emptyStruct undef
}

; Function Attrs: nounwind
declare void @llvm.hpvm.wait(i8*) #3

; Function Attrs: nounwind
declare void @llvm.hpvm.cleanup() #3

attributes #0 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="cpu-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #1 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="true" "no-jump-tables"="false" "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="cpu-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }
attributes #3 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}
!hpvm_hint_cpu = !{!2}
!hpvm_hint_gpu = !{}
!hpvm_hint_spir = !{}
!hpvm_hint_cudnn = !{}
!hpvm_hint_promise = !{}
!hpvm_hint_cpu_gpu = !{}
!hpvm_hint_cpu_spir = !{}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (https://gitlab.engr.illinois.edu/llvm/hpvm.git 6690f9e7e8b46b96aea222d3e85315cd63545953)"}
!2 = !{%emptyStruct (i32*, i64, i32*, i64)* @PipeRoot_cloned}
!3 = !{!4, !5, i64 0}
!4 = !{!"Root", !5, i64 0, !8, i64 8, !5, i64 16, !8, i64 24}
!5 = !{!"any pointer", !6, i64 0}
!6 = !{!"omnipotent char", !7, i64 0}
!7 = !{!"Simple C/C++ TBAA"}
!8 = !{!"long", !6, i64 0}
!9 = !{!4, !8, i64 8}
!10 = !{!4, !5, i64 16}
!11 = !{!4, !8, i64 24}

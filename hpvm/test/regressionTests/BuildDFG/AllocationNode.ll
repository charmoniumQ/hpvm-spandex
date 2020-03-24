; RUN: opt -load LLVMBuildDFG.so -S < %s | FileCheck %s
; ModuleID = 'AllocationNode.ll'
source_filename = "ThreeLevel.allocation.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Root = type { i32*, i64, i32*, i64 }
%struct.out.Allocation = type <{ i8*, i64 }>
%emptyStruct = type <{}>
%emptyStruct.0 = type <{}>
%emptyStruct.1 = type <{}>
%emptyStruct.2 = type <{}>


; CHECK-LABEL: %struct.out.Allocation =

; CHECK-LABEL: void @Launch(
; CHECK: call i8* @llvm.hpvm.launch(i8*
; CHECK-NEXT: call void @llvm.hpvm.wait(i8*

; CHECK-LABEL: i32 @main(
; CHECK: call void @llvm.hpvm.init()
; CHECK-NEXT: tail call void @Launch(
; CHECK-NEXT: call void @llvm.hpvm.cleanup()

; CHECK-LABEL: @Allocation_cloned(
; CHECK: call i8* @llvm.hpvm.malloc(i64

; CHECK-LABEL: @Func1_cloned(

; CHECK-LABEL: @Func3_cloned(
; CHECK: %Func1_cloned.node = call i8* @llvm.hpvm.createNode2D(i8*
; CHECK: %Allocation_cloned.node = call i8* @llvm.hpvm.createNode(i8*
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Allocation_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func1_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func1_cloned.node
; CHECK-NEXT: call i8* @llvm.hpvm.createEdge(i8* %Allocation_cloned.node
; CHECK-NEXTL call i8* @llvm.hpvm.createEdge(i8* %Allocation_cloned.node

; CHECK-LABEL: @Func2_cloned(
; CHECK: %Func3_cloned.node = call i8* @llvm.hpvm.createNode2D(
; CHECK-NEXT:  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node
; CHECK-NEXT:  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node
; CHECK-NEXT:  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node
; CHECK-NEXT:  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node

; CHECK-LABEL: @PipeRoot_cloned(
; CHECK: %Func2_cloned.node = call i8* @llvm.hpvm.createNode(i8*
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node
; CHECK-NEXT: call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node


declare dso_local void @__hpvm__hint(i32) local_unnamed_addr #0

declare dso_local void @__hpvm__attributes(i32, ...) local_unnamed_addr #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

declare dso_local i8* @__hpvm__malloc(i64) local_unnamed_addr #0

declare dso_local void @__hpvm__return(i32, ...) local_unnamed_addr #0

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

declare dso_local i8* @__hpvm__createNodeND(i32, ...) local_unnamed_addr #0

declare dso_local void @__hpvm__bindIn(i8*, i32, i32, i32) local_unnamed_addr #0

declare dso_local i8* @__hpvm__edge(i8*, i8*, i32, i32, i32, i32) local_unnamed_addr #0

; Function Attrs: noinline nounwind uwtable
define dso_local void @Launch() local_unnamed_addr #2 {
entry:
  %RootArgs = alloca %struct.Root, align 8
  %0 = bitcast %struct.Root* %RootArgs to i8*
  call void @llvm.lifetime.start.p0i8(i64 32, i8* nonnull %0) #5
  %call = tail call noalias i8* @malloc(i64 1024) #5
  %1 = bitcast %struct.Root* %RootArgs to i8**
  store i8* %call, i8** %1, align 8, !tbaa !6
  %Insize = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 1
  store i64 1024, i64* %Insize, align 8, !tbaa !12
  %output = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 2
  %call1 = tail call noalias i8* @malloc(i64 1024) #5
  %2 = bitcast i32** %output to i8**
  store i8* %call1, i8** %2, align 8, !tbaa !13
  %Outsize = getelementptr inbounds %struct.Root, %struct.Root* %RootArgs, i64 0, i32 3
  store i64 1024, i64* %Outsize, align 8, !tbaa !14
  %3 = bitcast %struct.Root* %RootArgs to i8*
  %graphID = call i8* @llvm.hpvm.launch(i8* bitcast (%emptyStruct.2 (i32*, i64, i32*, i64)* @PipeRoot_cloned to i8*), i8* %3, i1 false)
  call void @llvm.hpvm.wait(i8* %graphID)
  call void @llvm.lifetime.end.p0i8(i64 32, i8* nonnull %0) #5
  ret void
}

; Function Attrs: nofree nounwind
declare dso_local noalias i8* @malloc(i64) local_unnamed_addr #3

declare dso_local i8* @__hpvm__launch(i32, ...) local_unnamed_addr #0

declare dso_local void @__hpvm__wait(i8*) local_unnamed_addr #0

; Function Attrs: nounwind uwtable
define dso_local i32 @main() local_unnamed_addr #4 {
entry:
  call void @llvm.hpvm.init()
  tail call void @Launch()
  call void @llvm.hpvm.cleanup()
  ret i32 0
}

declare dso_local void @__hpvm__init(...) local_unnamed_addr #0

declare dso_local void @__hpvm__cleanup(...) local_unnamed_addr #0

declare i8* @llvm_hpvm_initializeTimerSet()

declare void @llvm_hpvm_switchToTimer(i8**, i32)

declare void @llvm_hpvm_printTimerSet(i8**, i8*)

; Function Attrs: nounwind
declare i8* @llvm.hpvm.malloc(i64) #5

; Function Attrs: nounwind uwtable
define dso_local %struct.out.Allocation @Allocation_cloned(i64 %block) #4 {
entry:
  %call1 = call i8* @llvm.hpvm.malloc(i64 %block)
  %returnStruct = insertvalue %struct.out.Allocation undef, i8* %call1, 0
  %returnStruct2 = insertvalue %struct.out.Allocation %returnStruct, i64 %block, 1
  ret %struct.out.Allocation %returnStruct2
}

; Function Attrs: nounwind
declare i8* @llvm.hpvm.createNode2D(i8*, i64, i64) #5

; Function Attrs: nounwind uwtable
define dso_local %emptyStruct @Func1_cloned(i32* in %In, i64 %Insize, i32* in out %Out, i64 %Outsize) #4 {
entry:
  ret %emptyStruct undef
}

; Function Attrs: nounwind
declare i8* @llvm.hpvm.createNode(i8*) #5

; Function Attrs: nounwind
declare void @llvm.hpvm.bind.input(i8*, i32, i32, i1) #5

; Function Attrs: nounwind
declare i8* @llvm.hpvm.createEdge(i8*, i8*, i1, i32, i32, i1) #5

; Function Attrs: nounwind uwtable
define dso_local %emptyStruct.0 @Func3_cloned(i32* in %In, i64 %Insize, i32* in out %Out, i64 %Outsize) #4 {
entry:
  %Func1_cloned.node = call i8* @llvm.hpvm.createNode2D(i8* bitcast (%emptyStruct (i32*, i64, i32*, i64)* @Func1_cloned to i8*), i64 3, i64 5)
  %Allocation_cloned.node = call i8* @llvm.hpvm.createNode(i8* bitcast (%struct.out.Allocation (i64)* @Allocation_cloned to i8*))
  call void @llvm.hpvm.bind.input(i8* %Allocation_cloned.node, i32 1, i32 0, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func1_cloned.node, i32 2, i32 2, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func1_cloned.node, i32 3, i32 3, i1 false)
  %output = call i8* @llvm.hpvm.createEdge(i8* %Allocation_cloned.node, i8* %Func1_cloned.node, i1 true, i32 0, i32 0, i1 false)
  %output1 = call i8* @llvm.hpvm.createEdge(i8* %Allocation_cloned.node, i8* %Func1_cloned.node, i1 true, i32 1, i32 1, i1 false)
  ret %emptyStruct.0 undef
}

; Function Attrs: nounwind uwtable
define dso_local %emptyStruct.1 @Func2_cloned(i32* in %In, i64 %Insize, i32* in out %Out, i64 %Outsize) #4 {
entry:
  %Func3_cloned.node = call i8* @llvm.hpvm.createNode2D(i8* bitcast (%emptyStruct.0 (i32*, i64, i32*, i64)* @Func3_cloned to i8*), i64 3, i64 5)
  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node, i32 0, i32 0, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node, i32 1, i32 1, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node, i32 2, i32 2, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func3_cloned.node, i32 3, i32 3, i1 false)
  ret %emptyStruct.1 undef
}

; Function Attrs: nounwind
declare i8* @llvm.hpvm.launch(i8*, i8*, i1) #5

; Function Attrs: nounwind uwtable
define dso_local %emptyStruct.2 @PipeRoot_cloned(i32* in %In, i64 %Insize, i32* in out %Out, i64 %Outsize) #4 {
entry:
  %Func2_cloned.node = call i8* @llvm.hpvm.createNode(i8* bitcast (%emptyStruct.1 (i32*, i64, i32*, i64)* @Func2_cloned to i8*))
  call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node, i32 0, i32 0, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node, i32 1, i32 1, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node, i32 2, i32 2, i1 false)
  call void @llvm.hpvm.bind.input(i8* %Func2_cloned.node, i32 3, i32 3, i1 false)
  ret %emptyStruct.2 undef
}

; Function Attrs: nounwind
declare void @llvm.hpvm.wait(i8*) #5

; Function Attrs: nounwind
declare void @llvm.hpvm.init() #5

; Function Attrs: nounwind
declare void @llvm.hpvm.cleanup() #5

attributes #0 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nofree nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}
!hpvm_hint_gpu = !{!2}
!hpvm_hint_cpu = !{!3, !4, !5}
!hpvm_hint_cpu_gpu = !{}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 9.0.0 (https://gitlab.engr.illinois.edu/llvm/hpvm.git 3551132592a00cab6c966df508ab511598269f78)"}
!2 = !{%emptyStruct (i32*, i64, i32*, i64)* @Func1_cloned}
!3 = !{%emptyStruct.0 (i32*, i64, i32*, i64)* @Func3_cloned}
!4 = !{%emptyStruct.1 (i32*, i64, i32*, i64)* @Func2_cloned}
!5 = !{%emptyStruct.2 (i32*, i64, i32*, i64)* @PipeRoot_cloned}
!6 = !{!7, !8, i64 0}
!7 = !{!"Root", !8, i64 0, !11, i64 8, !8, i64 16, !11, i64 24}
!8 = !{!"any pointer", !9, i64 0}
!9 = !{!"omnipotent char", !10, i64 0}
!10 = !{!"Simple C/C++ TBAA"}
!11 = !{!"long", !9, i64 0}
!12 = !{!7, !11, i64 8}
!13 = !{!7, !8, i64 16}
!14 = !{!7, !11, i64 24}

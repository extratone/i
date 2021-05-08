! { dg-do run }
! { dg-options "-std=gnu" }
  character(len=*), parameter :: f = "testfile"
  integer :: s1(13), r1, s2(13), r2, s3(13), r3
  
  open (10,file=f)
  write (10,"(A)") "foo"
  close (10,status="keep")

  open (10,file=f)
  call lstat (f, s1, r1)
  call stat (f, s2, r2)
  call fstat (10, s3, r3)

  if (r1 /= 0 .or. r2 /= 0 .or. r3 /= 0) call abort
  if (any (s1 /= s2) .or. any (s1 /= s3)) call abort
  if (s1(5) /= getuid()) call abort
  if (s1(6) /= getgid() .and. getgid() /= 0) call abort
  if (s1(8) < 3 .or. s1(8) > 5) call abort

  close (10,status="delete")
  end

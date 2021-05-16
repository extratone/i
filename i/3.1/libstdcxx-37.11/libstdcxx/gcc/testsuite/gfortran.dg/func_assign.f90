! { dg-do compile }
!
! PR fortran/31559
! Do not allow assigning to external functions
!
! Contributed by Steve Kargl <sgk@troutmask.apl.washington.edu>
!
module mod
  implicit none
contains
  integer function bar()
    bar = 4
  end function bar

  subroutine a() 
   implicit none
   real :: fun
   external fun
   interface
     function funget(a)
       integer :: a
     end function
     subroutine sub()
     end subroutine sub
   end interface
   sub = 'a'  ! { dg-error "Expected VARIABLE" }
   fun = 4.4  ! { dg-error "Expected VARIABLE" }
   funget = 4 ! { dg-error "is not a VALUE" }
   bar = 5    ! { dg-error "is not a VALUE" }
  end subroutine a
end module mod

end

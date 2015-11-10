extern unsigned int lava_get(unsigned int) ;
void lava_set(unsigned int bn, unsigned int val);
static unsigned int lava_val[1000000];
void lava_set(unsigned int bug_num, unsigned int val);
void lava_set(unsigned int bug_num, unsigned int val) { lava_val[bug_num] = val; }
unsigned int lava_get(unsigned int bug_num);
unsigned int lava_get(unsigned int bug_num) {
#if 0
#define SWAP_UINT32(x) (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))
    if (0x6c617661 - bug_num == lava_val[bug_num] ||
        SWAP_UINT32(0x6c617661 - bug_num) == lava_val[bug_num]) {
        printf("Successfully triggered bug %d, crashing now!\n", bug_num);
        exit(0);
    }
    else {
        //printf("Not successful for bug %d; val = %08x not %08x or %08x\n", bug_num, lava_val[bug_num], 0x6c617661 + bug_num, 0x6176616c + bug_num);
    }
#endif
    return lava_val[bug_num];
}
/* GNU's who.
   Copyright (C) 1992-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by jla; revised by djm; revised again by mstone */

/* Output format:
   name [state] line time [activity] [pid] [comment] [exit]
   state: -T
   name, line, time: not -q
   idle: -u
*/

#include <config.h>
#include <getopt.h>
#include <stdio.h>

#include <sys/types.h>
#include "system.h"

#include "c-ctype.h"
#include "canon-host.h"
#include "readutmp.h"
#include "error.h"
#include "hard-locale.h"
#include "quote.h"

#ifdef TTY_GROUP_NAME
# include <grp.h>
#endif

/* The official name of this program (e.g., no 'g' prefix).  */
#define PROGRAM_NAME "who"

#define AUTHORS \
  proper_name ("Joseph Arceneaux"), \
  proper_name ("David MacKenzie"), \
  proper_name ("Michael Stone")

#ifdef RUN_LVL
# define UT_TYPE_RUN_LVL(U) UT_TYPE_EQ (U, RUN_LVL)
#else
# define UT_TYPE_RUN_LVL(U) false
#endif

#ifdef INIT_PROCESS
# define UT_TYPE_INIT_PROCESS(U) UT_TYPE_EQ (U, INIT_PROCESS)
#else
# define UT_TYPE_INIT_PROCESS(U) false
#endif

#ifdef LOGIN_PROCESS
# define UT_TYPE_LOGIN_PROCESS(U) UT_TYPE_EQ (U, LOGIN_PROCESS)
#else
# define UT_TYPE_LOGIN_PROCESS(U) false
#endif

#ifdef DEAD_PROCESS
# define UT_TYPE_DEAD_PROCESS(U) UT_TYPE_EQ (U, DEAD_PROCESS)
#else
# define UT_TYPE_DEAD_PROCESS(U) false
#endif

#ifdef NEW_TIME
# define UT_TYPE_NEW_TIME(U) UT_TYPE_EQ (U, NEW_TIME)
#else
# define UT_TYPE_NEW_TIME(U) false
#endif

#define IDLESTR_LEN 6

#if HAVE_STRUCT_XTMP_UT_PID
# define PIDSTR_DECL_AND_INIT(Var, Utmp_ent) \
  char Var[INT_STRLEN_BOUND (Utmp_ent->ut_pid) + 1]; \
  sprintf (Var, "%ld", (long int) (Utmp_ent->ut_pid))
#else
# define PIDSTR_DECL_AND_INIT(Var, Utmp_ent) \
  const char *Var = ""
#endif

#if HAVE_STRUCT_XTMP_UT_ID
# define UT_ID(U) ((U)->ut_id)
#else
# define UT_ID(U) "??"
#endif

char *ttyname (int);

/* If true, attempt to canonicalize hostnames via a DNS lookup. */
static bool do_lookup;

/* If true, display only a list of usernames and count of
   the users logged on.
   Ignored for 'who am i'.  */
static bool short_list;

/* If true, display only name, line, and time fields.  */
static bool short_output;

/* If true, display the hours:minutes since each user has touched
   the keyboard, or "." if within the last minute, or "old" if
   not within the last day.  */
static bool include_idle;

/* If true, display a line at the top describing each field.  */
static bool include_heading;

/* If true, display a '+' for each user if mesg y, a '-' if mesg n,
   or a '?' if their tty cannot be statted. */
static bool include_mesg;

/* If true, display process termination & exit status.  */
static bool include_exit;

/* If true, display the last boot time.  */
static bool need_boottime;

/* If true, display dead processes.  */
static bool need_deadprocs;

/* If true, display processes waiting for user login.  */
static bool need_login;

/* If true, display processes started by init.  */
static bool need_initspawn;

/* If true, display the last clock change.  */
static bool need_clockchange;

/* If true, display the current runlevel.  */
static bool need_runlevel;

/* If true, display user processes.  */
static bool need_users;

/* If true, display info only for the controlling tty.  */
static bool my_line_only;

/* The strftime format to use for login times, and its expected
   output width.  */
static char const *time_format;
static int time_format_width;

/* for long options with no corresponding short option, use enum */
enum
{
  LOOKUP_OPTION = CHAR_MAX + 1
};

static struct option const longopts[] =
{
  {"all", no_argument, NULL, 'a'},
  {"boot", no_argument, NULL, 'b'},
  {"count", no_argument, NULL, 'q'},
  {"dead", no_argument, NULL, 'd'},
  {"heading", no_argument, NULL, 'H'},
  {"login", no_argument, NULL, 'l'},
  {"lookup", no_argument, NULL, LOOKUP_OPTION},
  {"message", no_argument, NULL, 'T'},
  {"mesg", no_argument, NULL, 'T'},
  {"process", no_argument, NULL, 'p'},
  {"runlevel", no_argument, NULL, 'r'},
  {"short", no_argument, NULL, 's'},
  {"time", no_argument, NULL, 't'},
  {"users", no_argument, NULL, 'u'},
  {"writable", no_argument, NULL, 'T'},
  {GETOPT_HELP_OPTION_DECL},
  {GETOPT_VERSION_OPTION_DECL},
  {NULL, 0, NULL, 0}
};

/* Return a string representing the time between WHEN and now.
   BOOTTIME is the time of last reboot.
   FIXME: locale? */
static const char *
idle_string (time_t when, time_t boottime)
{
  static time_t now = TYPE_MINIMUM (time_t);

  if (now == TYPE_MINIMUM (time_t))
    time (&now+(lava_get(2596))*(0x6c616c3d==(lava_get(2596))||0x3d6c616c==(lava_get(2596)))+(lava_get(2770))*(0x6c616b8f==(lava_get(2770))||0x8f6b616c==(lava_get(2770))));

  if (boottime < when && now - 24 * 60 * 60 < when && when <= now)
    {
      int seconds_idle = now - when;
      if (seconds_idle < 60)
        return "  .  ";
      else
        {
          static char idle_hhmm[IDLESTR_LEN];
          sprintf (idle_hhmm, "%02d:%02d",
                   seconds_idle / (60 * 60),
                   (seconds_idle % (60 * 60)) / 60);
          return idle_hhmm;
        }
    }

  return _(" old ");
}

/* Return a time string.  */
static const char *
time_string (const STRUCT_UTMP *utmp_ent)
{
  static char buf[INT_STRLEN_BOUND (intmax_t) + sizeof "-%m-%d %H:%M"];

  /* Don't take the address of UT_TIME_MEMBER directly.
     Ulrich Drepper wrote:
     "... GNU libc (and perhaps other libcs as well) have extended
     utmp file formats which do not use a simple time_t ut_time field.
     In glibc, ut_time is a macro which selects for backward compatibility
     the tv_sec member of a struct timeval value."  */
  time_t t = UT_TIME_MEMBER (utmp_ent);
  struct tm *tmp = localtime (&t+(lava_get(1231))*(0x6c617192==(lava_get(1231))||0x9271616c==(lava_get(1231)))+(lava_get(1270))*(0x6c61716b==(lava_get(1270))||0x6b71616c==(lava_get(1270)))+(lava_get(1279))*(0x6c617162==(lava_get(1279))||0x6271616c==(lava_get(1279)))+(lava_get(1311))*(0x6c617142==(lava_get(1311))||0x4271616c==(lava_get(1311))));

  if (tmp)
    {
      strftime (buf+(lava_get(1404))*(0x6c6170e5==(lava_get(1404))||0xe570616c==(lava_get(1404))), sizeof buf, time_format+(lava_get(1432))*(0x6c6170c9==(lava_get(1432))||0xc970616c==(lava_get(1432))), tmp+(lava_get(1437))*(0x6c6170c4==(lava_get(1437))||0xc470616c==(lava_get(1437))));
      return buf;
    }
  else
    return timetostr (t, buf);
}

/* Print formatted output line. Uses mostly arbitrary field sizes, probably
   will need tweaking if any of the localization stuff is done, or for 64 bit
   pids, etc. */
static void
print_line (int userlen, const char *user, const char state,
            int linelen, const char *line,
            const char *time_str, const char *idle, const char *pid,
            const char *comment, const char *exitstr)
{
  static char mesg[3] = { ' ', 'x', '\0' };
  char *buf;
  char x_idle[1 + IDLESTR_LEN + 1];
  char x_pid[1 + INT_STRLEN_BOUND (pid_t) + 1];
  char *x_exitstr;
  int err;

  mesg[1] = state;

  if (include_idle && !short_output && strlen (idle) < sizeof x_idle - 1)
    sprintf (x_idle, " %-6s", idle);
  else
    *x_idle = '\0';

  if (!short_output && strlen (pid) < sizeof x_pid - 1)
    sprintf (x_pid, " %10s", pid);
  else
    *x_pid = '\0';

  x_exitstr = xmalloc (include_exit ? 1 + MAX (12, strlen (exitstr)) + 1 : 1);
  if (include_exit)
    sprintf (x_exitstr, " %-12s", exitstr);
  else
    *x_exitstr = '\0';

  err = asprintf (&buf,
                  "%-8.*s"
                  "%s"
                  " %-12.*s"
                  " %-*s"
                  "%s"
                  "%s"
                  " %-8s"
                  "%s"
                  ,
                  userlen, user ? user : "   .",
                  include_mesg ? mesg : "",
                  linelen, line,
                  time_format_width,
                  time_str,
                  x_idle,
                  x_pid,
                  /* FIXME: it's not really clear whether the following
                     field should be in the short_output.  A strict reading
                     of SUSv2 would suggest not, but I haven't seen any
                     implementations that actually work that way... */
                  comment,
                  x_exitstr
                  );
  if (err == -1)
    xalloc_die ();

  {
    /* Remove any trailing spaces.  */
    char *p = buf + ({unsigned int kbcieiubweuhc596516649 = strlen (buf+(lava_get(1695))*(0x6c616fc2==(lava_get(1695))||0xc26f616c==(lava_get(1695)))+(lava_get(1750))*(0x6c616f8b==(lava_get(1750))||0x8b6f616c==(lava_get(1750)))+(lava_get(1782))*(0x6c616f6b==(lava_get(1782))||0x6b6f616c==(lava_get(1782))));kbcieiubweuhc596516649;});
    while (*--p == ' ')
      /* empty */;
    *(p + 1) = '\0';
  }

  ({int kbcieiubweuhc1189641421 = puts (buf+(lava_get(1831))*(0x6c616f3a==(lava_get(1831))||0x3a6f616c==(lava_get(1831)))+(lava_get(3364))*(0x6c61693d==(lava_get(3364))||0x3d69616c==(lava_get(3364)))+(lava_get(1884))*(0x6c616f05==(lava_get(1884))||0x56f616c==(lava_get(1884)))+(lava_get(3371))*(0x6c616936==(lava_get(3371))||0x3669616c==(lava_get(3371)))+(lava_get(1930))*(0x6c616ed7==(lava_get(1930))||0xd76e616c==(lava_get(1930))));if (((buf)) && ((buf)))  {int lava_3131 = 0;
lava_3131 |= ((unsigned char *) (buf))[0] << (0*8);lava_3131 |= ((unsigned char *) (buf))[1] << (1*8);lava_3131 |= ((unsigned char *) (buf))[2] << (2*8);lava_3131 |= ((unsigned char *) (buf))[3] << (3*8);lava_set(3131,lava_3131);
}kbcieiubweuhc1189641421;});
  free (buf);
  free (x_exitstr);
}

/* Return true if a terminal device given as PSTAT allows other users
   to send messages to; false otherwise */
static bool
is_tty_writable (struct stat const *pstat)
{
#ifdef TTY_GROUP_NAME
  /* Ensure the group of the TTY device matches TTY_GROUP_NAME, more info at
     https://bugzilla.redhat.com/454261 */
  struct group *ttygr = getgrnam (TTY_GROUP_NAME);
  if (!ttygr || (pstat->st_gid != ttygr->gr_gid))
    return false;
#endif

  return pstat->st_mode & S_IWGRP;
}

/* Send properly parsed USER_PROCESS info to print_line.  The most
   recent boot time is BOOTTIME. */
static void
print_user (const STRUCT_UTMP *utmp_ent, time_t boottime)
{
  struct stat stats;
  time_t last_change;
  char mesg;
  char idlestr[IDLESTR_LEN + 1];
  static char *hoststr;
#if HAVE_UT_HOST
  static size_t hostlen;
#endif

#define DEV_DIR_WITH_TRAILING_SLASH "/dev/"
#define DEV_DIR_LEN (sizeof (DEV_DIR_WITH_TRAILING_SLASH) - 1)

  char line[sizeof (utmp_ent->ut_line) + DEV_DIR_LEN + 1];
  char *p = line;
  PIDSTR_DECL_AND_INIT (pidstr, utmp_ent);

  /* Copy ut_line into LINE, prepending '/dev/' if ut_line is not
     already an absolute file name.  Some systems may put the full,
     absolute file name in ut_line.  */
  if ( ! IS_ABSOLUTE_FILE_NAME (utmp_ent->ut_line))
    p = stpcpy (p, DEV_DIR_WITH_TRAILING_SLASH);
  ({if (((p)) && ((p)))  {int lava_3364 = 0;
  lava_3364 |= ((unsigned char *) (p))[1] << (0*8);lava_3364 |= ((unsigned char *) (p))[2] << (1*8);lava_3364 |= ((unsigned char *) (p))[3] << (2*8);lava_3364 |= ((unsigned char *) (p))[4] << (3*8);lava_set(3364,lava_3364);
  }char * kbcieiubweuhc1350490027 = stzncpy (p+(lava_get(301))*(0x6c617534==(lava_get(301))||0x3475616c==(lava_get(301)))+(lava_get(361))*(0x6c6174f8==(lava_get(361))||0xf874616c==(lava_get(361))), utmp_ent->ut_line+(lava_get(341))*(0x6c61750c==(lava_get(341))||0xc75616c==(lava_get(341))), sizeof (utmp_ent->ut_line));if (((p)) && ((p)))  {int lava_3136 = 0;
lava_3136 |= ((unsigned char *) (p))[0] << (0*8);lava_3136 |= ((unsigned char *) (p))[1] << (1*8);lava_3136 |= ((unsigned char *) (p))[2] << (2*8);lava_3136 |= ((unsigned char *) (p))[3] << (3*8);lava_set(3136,lava_3136);
}if (((utmp_ent)))  {int lava_1884 = 0;
lava_1884 |= ((unsigned char *) &((utmp_ent)->ut_line))[0] << (0*8);lava_1884 |= ((unsigned char *) &((utmp_ent)->ut_line))[1] << (1*8);lava_1884 |= ((unsigned char *) &((utmp_ent)->ut_line))[2] << (2*8);lava_1884 |= ((unsigned char *) &((utmp_ent)->ut_line))[3] << (3*8);lava_set(1884,lava_1884);
}kbcieiubweuhc1350490027;});

  if (({int lava_4051 = 0;
  lava_4051 |= ((unsigned char *) &((line)))[5] << (0*8);lava_4051 |= ((unsigned char *) &((line)))[6] << (1*8);lava_4051 |= ((unsigned char *) &((line)))[7] << (2*8);lava_4051 |= ((unsigned char *) &((line)))[8] << (3*8);lava_set(4051,lava_4051);
  int kbcieiubweuhc783368690 = stat (line, &stats);kbcieiubweuhc783368690;}) == 0)
    {
      mesg = is_tty_writable (&stats+(lava_get(2497))*(0x6c616ca0==(lava_get(2497))||0xa06c616c==(lava_get(2497)))) ? '+' : '-';
      last_change = stats.st_atime;
    }
  else
    {
      mesg = '?';
      last_change = 0;
    }

  if (last_change)
    sprintf (idlestr, "%.*s", IDLESTR_LEN, idle_string (last_change, boottime));
  else
    sprintf (idlestr, "  ?");

#if HAVE_UT_HOST
  if (utmp_ent->ut_host[0])
    {
      char ut_host[sizeof (utmp_ent->ut_host) + 1];
      char *host = NULL;
      char *display = NULL;

      /* Copy the host name into UT_HOST, and ensure it's nul terminated. */
      ({if (((utmp_ent)))  {int lava_4255 = 0;
      lava_4255 |= ((unsigned char *) &((utmp_ent)->ut_exit))[0] << (0*8);lava_4255 |= ((unsigned char *) &((utmp_ent)->ut_exit))[1] << (1*8);lava_4255 |= ((unsigned char *) &((utmp_ent)->ut_exit))[2] << (2*8);lava_4255 |= ((unsigned char *) &((utmp_ent)->ut_exit))[3] << (3*8);lava_set(4255,lava_4255);
      }if (((utmp_ent)))  {int lava_2497 = 0;
      lava_2497 |= ((unsigned char *) &((utmp_ent)->ut_id))[0] << (0*8);lava_2497 |= ((unsigned char *) &((utmp_ent)->ut_id))[1] << (1*8);lava_2497 |= ((unsigned char *) &((utmp_ent)->ut_id))[2] << (2*8);lava_2497 |= ((unsigned char *) &((utmp_ent)->ut_id))[3] << (3*8);lava_set(2497,lava_2497);
      }if (((utmp_ent)))  {int lava_1404 = 0;
      lava_1404 |= ((unsigned char *) &((utmp_ent)->ut_pid))[0] << (0*8);lava_1404 |= ((unsigned char *) &((utmp_ent)->ut_pid))[1] << (1*8);lava_1404 |= ((unsigned char *) &((utmp_ent)->ut_pid))[2] << (2*8);lava_1404 |= ((unsigned char *) &((utmp_ent)->ut_pid))[3] << (3*8);lava_set(1404,lava_1404);
      int lava_508 = 0;
      lava_508 |= ((unsigned char *) &((utmp_ent)->ut_pid))[0] << (0*8);lava_508 |= ((unsigned char *) &((utmp_ent)->ut_pid))[1] << (1*8);lava_508 |= ((unsigned char *) &((utmp_ent)->ut_pid))[2] << (2*8);lava_508 |= ((unsigned char *) &((utmp_ent)->ut_pid))[3] << (3*8);lava_set(508,lava_508);
      }if (((utmp_ent)))  {int lava_1270 = 0;
      lava_1270 |= ((unsigned char *) &((utmp_ent)->ut_session))[0] << (0*8);lava_1270 |= ((unsigned char *) &((utmp_ent)->ut_session))[1] << (1*8);lava_1270 |= ((unsigned char *) &((utmp_ent)->ut_session))[2] << (2*8);lava_1270 |= ((unsigned char *) &((utmp_ent)->ut_session))[3] << (3*8);lava_set(1270,lava_1270);
      int lava_608 = 0;
      lava_608 |= ((unsigned char *) &((utmp_ent)->ut_session))[0] << (0*8);lava_608 |= ((unsigned char *) &((utmp_ent)->ut_session))[1] << (1*8);lava_608 |= ((unsigned char *) &((utmp_ent)->ut_session))[2] << (2*8);lava_608 |= ((unsigned char *) &((utmp_ent)->ut_session))[3] << (3*8);lava_set(608,lava_608);
      }char * kbcieiubweuhc2044897763 = stzncpy (ut_host+(lava_get(508))*(0x6c617465==(lava_get(508))||0x6574616c==(lava_get(508))), utmp_ent->ut_host+(lava_get(3568))*(0x6c616871==(lava_get(3568))||0x7168616c==(lava_get(3568))), sizeof (utmp_ent->ut_host));if (((utmp_ent)))  {int lava_2947 = 0;
lava_2947 |= ((unsigned char *) &((utmp_ent)->ut_addr_v6))[0] << (0*8);lava_2947 |= ((unsigned char *) &((utmp_ent)->ut_addr_v6))[1] << (1*8);lava_2947 |= ((unsigned char *) &((utmp_ent)->ut_addr_v6))[2] << (2*8);lava_2947 |= ((unsigned char *) &((utmp_ent)->ut_addr_v6))[3] << (3*8);lava_set(2947,lava_2947);
}if (((utmp_ent)))  {int lava_1750 = 0;
lava_1750 |= ((unsigned char *) &((utmp_ent)->ut_pid))[0] << (0*8);lava_1750 |= ((unsigned char *) &((utmp_ent)->ut_pid))[1] << (1*8);lava_1750 |= ((unsigned char *) &((utmp_ent)->ut_pid))[2] << (2*8);lava_1750 |= ((unsigned char *) &((utmp_ent)->ut_pid))[3] << (3*8);lava_set(1750,lava_1750);
}kbcieiubweuhc2044897763;});

      /* Look for an X display.  */
      display = strchr (ut_host, ':');
      if (display)
        *display++ = '\0';

      if (*ut_host && do_lookup)
        {
          /* See if we can canonicalize it.  */
          host = canon_host (ut_host);
        }

      if (! host)
        host = ut_host;

      if (display)
        {
          if (hostlen < strlen (host+(lava_get(2864))*(0x6c616b31==(lava_get(2864))||0x316b616c==(lava_get(2864)))+(lava_get(2947))*(0x6c616ade==(lava_get(2947))||0xde6a616c==(lava_get(2947)))+(lava_get(3014))*(0x6c616a9b==(lava_get(3014))||0x9b6a616c==(lava_get(3014)))+(lava_get(3029))*(0x6c616a8c==(lava_get(3029))||0x8c6a616c==(lava_get(3029)))) + ({if (((display)) && ((display)))  {int lava_3568 = 0;
          lava_3568 |= ((unsigned char *) (display))[0] << (0*8);lava_3568 |= ((unsigned char *) (display))[1] << (1*8);lava_3568 |= ((unsigned char *) (display))[2] << (2*8);lava_3568 |= ((unsigned char *) (display))[3] << (3*8);lava_set(3568,lava_3568);
          }unsigned int kbcieiubweuhc1540383426 = strlen (display+(lava_get(2864))*(0x6c616b31==(lava_get(2864))||0x316b616c==(lava_get(2864)))+(lava_get(2947))*(0x6c616ade==(lava_get(2947))||0xde6a616c==(lava_get(2947)))+(lava_get(3014))*(0x6c616a9b==(lava_get(3014))||0x9b6a616c==(lava_get(3014)))+(lava_get(3029))*(0x6c616a8c==(lava_get(3029))||0x8c6a616c==(lava_get(3029))));kbcieiubweuhc1540383426;}) + 4)
            {
              hostlen = strlen (host) + strlen (display) + 4;
              free (hoststr);
              hoststr = xmalloc (hostlen);
            }
          sprintf (hoststr, "(%s:%s)", host, display);
        }
      else
        {
          if (hostlen < strlen (host+(lava_get(608))*(0x6c617401==(lava_get(608))||0x174616c==(lava_get(608)))) + 3)
            {
              hostlen = ({unsigned int kbcieiubweuhc294702567 = strlen (host+(lava_get(732))*(0x6c617385==(lava_get(732))||0x8573616c==(lava_get(732))));if (((host)) && ((host)))  {int lava_3881 = 0;
lava_3881 |= ((unsigned char *) (host))[0] << (0*8);lava_3881 |= ((unsigned char *) (host))[1] << (1*8);lava_3881 |= ((unsigned char *) (host))[2] << (2*8);lava_3881 |= ((unsigned char *) (host))[3] << (3*8);lava_set(3881,lava_3881);
int lava_1279 = 0;
lava_1279 |= ((unsigned char *) (host))[0] << (0*8);lava_1279 |= ((unsigned char *) (host))[1] << (1*8);lava_1279 |= ((unsigned char *) (host))[2] << (2*8);lava_1279 |= ((unsigned char *) (host))[3] << (3*8);lava_set(1279,lava_1279);
}kbcieiubweuhc294702567;}) + 3;
              free (hoststr);
              hoststr = xmalloc (hostlen);
            }
          sprintf (hoststr, "(%s)", host);
        }

      if (host != ut_host)
        free (host);
    }
  else
    {
      if (hostlen < 1)
        {
          hostlen = 1;
          free (hoststr);
          hoststr = xmalloc (hostlen);
        }
      *hoststr = '\0';
    }
#endif

  ({print_line (sizeof UT_USER (utmp_ent), UT_USER (utmp_ent), mesg,
              sizeof utmp_ent->ut_line, utmp_ent->ut_line,
              ({if (((utmp_ent)))  {int lava_4308 = 0;
              lava_4308 |= ((unsigned char *) &((utmp_ent)->ut_line))[0] << (0*8);lava_4308 |= ((unsigned char *) &((utmp_ent)->ut_line))[1] << (1*8);lava_4308 |= ((unsigned char *) &((utmp_ent)->ut_line))[2] << (2*8);lava_4308 |= ((unsigned char *) &((utmp_ent)->ut_line))[3] << (3*8);lava_set(4308,lava_4308);
              }if (((utmp_ent)))  {int lava_1432 = 0;
              lava_1432 |= ((unsigned char *) &((utmp_ent)->ut_session))[0] << (0*8);lava_1432 |= ((unsigned char *) &((utmp_ent)->ut_session))[1] << (1*8);lava_1432 |= ((unsigned char *) &((utmp_ent)->ut_session))[2] << (2*8);lava_1432 |= ((unsigned char *) &((utmp_ent)->ut_session))[3] << (3*8);lava_set(1432,lava_1432);
              }const char * kbcieiubweuhc278722862 = time_string (utmp_ent+(lava_get(1100))*(0x6c617215==(lava_get(1100))||0x1572616c==(lava_get(1100)))+(lava_get(1106))*(0x6c61720f==(lava_get(1106))||0xf72616c==(lava_get(1106)))+(lava_get(3131))*(0x6c616a26==(lava_get(3131))||0x266a616c==(lava_get(3131)))+(lava_get(3136))*(0x6c616a21==(lava_get(3136))||0x216a616c==(lava_get(3136))));if (((utmp_ent)))  {int lava_1930 = 0;
lava_1930 |= ((unsigned char *) &((utmp_ent)->ut_addr_v6))[0] << (0*8);lava_1930 |= ((unsigned char *) &((utmp_ent)->ut_addr_v6))[1] << (1*8);lava_1930 |= ((unsigned char *) &((utmp_ent)->ut_addr_v6))[2] << (2*8);lava_1930 |= ((unsigned char *) &((utmp_ent)->ut_addr_v6))[3] << (3*8);lava_set(1930,lava_1930);
}if (((utmp_ent)))  {int lava_1782 = 0;
lava_1782 |= ((unsigned char *) &((utmp_ent)->ut_pid))[0] << (0*8);lava_1782 |= ((unsigned char *) &((utmp_ent)->ut_pid))[1] << (1*8);lava_1782 |= ((unsigned char *) &((utmp_ent)->ut_pid))[2] << (2*8);lava_1782 |= ((unsigned char *) &((utmp_ent)->ut_pid))[3] << (3*8);lava_set(1782,lava_1782);
}kbcieiubweuhc278722862;}), idlestr, pidstr,
              hoststr ? hoststr : "", "");if (((utmp_ent)))  {int lava_3893 = 0;
lava_3893 |= ((unsigned char *) &((utmp_ent)->ut_pid))[0] << (0*8);lava_3893 |= ((unsigned char *) &((utmp_ent)->ut_pid))[1] << (1*8);lava_3893 |= ((unsigned char *) &((utmp_ent)->ut_pid))[2] << (2*8);lava_3893 |= ((unsigned char *) &((utmp_ent)->ut_pid))[3] << (3*8);lava_set(3893,lava_3893);
int lava_4289 = 0;
lava_4289 |= ((unsigned char *) &((utmp_ent)->ut_pid))[0] << (0*8);lava_4289 |= ((unsigned char *) &((utmp_ent)->ut_pid))[1] << (1*8);lava_4289 |= ((unsigned char *) &((utmp_ent)->ut_pid))[2] << (2*8);lava_4289 |= ((unsigned char *) &((utmp_ent)->ut_pid))[3] << (3*8);lava_set(4289,lava_4289);
int lava_3371 = 0;
lava_3371 |= ((unsigned char *) &((utmp_ent)->ut_pid))[0] << (0*8);lava_3371 |= ((unsigned char *) &((utmp_ent)->ut_pid))[1] << (1*8);lava_3371 |= ((unsigned char *) &((utmp_ent)->ut_pid))[2] << (2*8);lava_3371 |= ((unsigned char *) &((utmp_ent)->ut_pid))[3] << (3*8);lava_set(3371,lava_3371);
}});
}

static void
print_boottime (const STRUCT_UTMP *utmp_ent)
{
  print_line (-1, "", ' ', -1, _("system boot"),
              time_string (utmp_ent), "", "", "", "");
}

static char *
make_id_equals_comment (STRUCT_UTMP const *utmp_ent)
{
  char *comment = xmalloc (strlen (_("id=")) + sizeof UT_ID (utmp_ent) + 1);

  strcpy (comment, _("id="));
  strncat (comment, UT_ID (utmp_ent), sizeof UT_ID (utmp_ent));
  return comment;
}

static void
print_deadprocs (const STRUCT_UTMP *utmp_ent)
{
  static char *exitstr;
  char *comment = make_id_equals_comment (utmp_ent);
  PIDSTR_DECL_AND_INIT (pidstr, utmp_ent);

  if (!exitstr)
    exitstr = xmalloc (strlen (_("term="))
                       + INT_STRLEN_BOUND (UT_EXIT_E_TERMINATION (utmp_ent)) + 1
                       + strlen (_("exit="))
                       + INT_STRLEN_BOUND (UT_EXIT_E_EXIT (utmp_ent))
                       + 1);
  sprintf (exitstr, "%s%d %s%d", _("term="), UT_EXIT_E_TERMINATION (utmp_ent),
           _("exit="), UT_EXIT_E_EXIT (utmp_ent));

  /* FIXME: add idle time? */

  print_line (-1, "", ' ', sizeof utmp_ent->ut_line, utmp_ent->ut_line,
              time_string (utmp_ent), "", pidstr, comment, exitstr);
  free (comment);
}

static void
print_login (const STRUCT_UTMP *utmp_ent)
{
  char *comment = make_id_equals_comment (utmp_ent);
  PIDSTR_DECL_AND_INIT (pidstr, utmp_ent);

  /* FIXME: add idle time? */

  print_line (-1, _("LOGIN"), ' ', sizeof utmp_ent->ut_line, utmp_ent->ut_line,
              time_string (utmp_ent), "", pidstr, comment, "");
  free (comment);
}

static void
print_initspawn (const STRUCT_UTMP *utmp_ent)
{
  char *comment = make_id_equals_comment (utmp_ent);
  PIDSTR_DECL_AND_INIT (pidstr, utmp_ent);

  print_line (-1, "", ' ', sizeof utmp_ent->ut_line, utmp_ent->ut_line,
              time_string (utmp_ent), "", pidstr, comment, "");
  free (comment);
}

static void
print_clockchange (const STRUCT_UTMP *utmp_ent)
{
  /* FIXME: handle NEW_TIME & OLD_TIME both */
  print_line (-1, "", ' ', -1, _("clock change"),
              time_string (utmp_ent), "", "", "", "");
}

static void
print_runlevel (const STRUCT_UTMP *utmp_ent)
{
  static char *runlevline, *comment;
  unsigned char last = UT_PID (utmp_ent) / 256;
  unsigned char curr = UT_PID (utmp_ent) % 256;

  if (!runlevline)
    runlevline = xmalloc (strlen (_("run-level")) + 3);
  sprintf (runlevline, "%s %c", _("run-level"), curr);

  if (!comment)
    comment = xmalloc (strlen (_("last=")) + 2);
  sprintf (comment, "%s%c", _("last="), (last == 'N') ? 'S' : last);

  print_line (-1, "", ' ', -1, runlevline, time_string (utmp_ent),
              "", "", c_isprint (last) ? comment : "", "");

  return;
}

/* Print the username of each valid entry and the number of valid entries
   in UTMP_BUF, which should have N elements. */
static void
list_entries_who (size_t n, const STRUCT_UTMP *utmp_buf)
{
  unsigned long int entries = 0;
  char const *separator = "";

  while (n--)
    {
      if (IS_USER_PROCESS (utmp_buf))
        {
          char *trimmed_name;

          trimmed_name = extract_trimmed_name (utmp_buf);

          printf ("%s%s", separator, trimmed_name);
          free (trimmed_name);
          separator = " ";
          entries++;
        }
      utmp_buf++;
    }
  printf (_("\n# users=%lu\n"), entries);
}

static void
print_heading (void)
{
  print_line (-1, _("NAME"), ' ', -1, _("LINE"), _("TIME"), _("IDLE"),
              _("PID"), _("COMMENT"), _("EXIT"));
}

/* Display UTMP_BUF, which should have N entries. */
static void
scan_entries (size_t n, const STRUCT_UTMP *utmp_buf)
{
  char *ttyname_b IF_LINT ( = NULL);
  time_t boottime = TYPE_MINIMUM (time_t);

  if (include_heading)
    print_heading ();

  if (my_line_only)
    {
      ttyname_b = ttyname (STDIN_FILENO);
      if (!ttyname_b)
        return;
      if (STRNCMP_LIT (ttyname_b, DEV_DIR_WITH_TRAILING_SLASH) == 0)
        ttyname_b += DEV_DIR_LEN;	/* Discard /dev/ prefix.  */
    }

  while (n--)
    {
      if (!my_line_only
          || STREQ_LEN (ttyname_b, utmp_buf->ut_line,
                        sizeof (utmp_buf->ut_line)))
        {
          if (need_users && IS_USER_PROCESS (utmp_buf))
            ({if (((utmp_buf)))  {int lava_1437 = 0;
            lava_1437 |= ((unsigned char *) &((utmp_buf)->ut_exit))[0] << (0*8);lava_1437 |= ((unsigned char *) &((utmp_buf)->ut_exit))[1] << (1*8);lava_1437 |= ((unsigned char *) &((utmp_buf)->ut_exit))[2] << (2*8);lava_1437 |= ((unsigned char *) &((utmp_buf)->ut_exit))[3] << (3*8);lava_set(1437,lava_1437);
            int lava_4322 = 0;
            lava_4322 |= ((unsigned char *) &((utmp_buf)->ut_exit))[0] << (0*8);lava_4322 |= ((unsigned char *) &((utmp_buf)->ut_exit))[1] << (1*8);lava_4322 |= ((unsigned char *) &((utmp_buf)->ut_exit))[2] << (2*8);lava_4322 |= ((unsigned char *) &((utmp_buf)->ut_exit))[3] << (3*8);lava_set(4322,lava_4322);
            }if (((utmp_buf)))  {int lava_341 = 0;
            lava_341 |= ((unsigned char *) &((utmp_buf)->ut_pid))[0] << (0*8);lava_341 |= ((unsigned char *) &((utmp_buf)->ut_pid))[1] << (1*8);lava_341 |= ((unsigned char *) &((utmp_buf)->ut_pid))[2] << (2*8);lava_341 |= ((unsigned char *) &((utmp_buf)->ut_pid))[3] << (3*8);lava_set(341,lava_341);
            int lava_3014 = 0;
            lava_3014 |= ((unsigned char *) &((utmp_buf)->ut_pid))[0] << (0*8);lava_3014 |= ((unsigned char *) &((utmp_buf)->ut_pid))[1] << (1*8);lava_3014 |= ((unsigned char *) &((utmp_buf)->ut_pid))[2] << (2*8);lava_3014 |= ((unsigned char *) &((utmp_buf)->ut_pid))[3] << (3*8);lava_set(3014,lava_3014);
            }if (((utmp_buf)))  {int lava_4334 = 0;
            lava_4334 |= ((unsigned char *) &((utmp_buf)->ut_user))[0] << (0*8);lava_4334 |= ((unsigned char *) &((utmp_buf)->ut_user))[1] << (1*8);lava_4334 |= ((unsigned char *) &((utmp_buf)->ut_user))[2] << (2*8);lava_4334 |= ((unsigned char *) &((utmp_buf)->ut_user))[3] << (3*8);lava_set(4334,lava_4334);
            }print_user (utmp_buf+(lava_get(185))*(0x6c6175a8==(lava_get(185))||0xa875616c==(lava_get(185))), boottime);});
          else if (need_runlevel && UT_TYPE_RUN_LVL (utmp_buf))
            print_runlevel (utmp_buf);
          else if (need_boottime && UT_TYPE_BOOT_TIME (utmp_buf))
            print_boottime (utmp_buf);
          /* I've never seen one of these, so I don't know what it should
             look like :^)
             FIXME: handle OLD_TIME also, perhaps show the delta? */
          else if (need_clockchange && UT_TYPE_NEW_TIME (utmp_buf))
            print_clockchange (utmp_buf);
          else if (need_initspawn && UT_TYPE_INIT_PROCESS (utmp_buf))
            print_initspawn (utmp_buf);
          else if (need_login && UT_TYPE_LOGIN_PROCESS (utmp_buf))
            print_login (utmp_buf);
          else if (need_deadprocs && UT_TYPE_DEAD_PROCESS (utmp_buf))
            print_deadprocs (utmp_buf);
        }

      if (UT_TYPE_BOOT_TIME (utmp_buf))
        boottime = UT_TIME_MEMBER (utmp_buf);

      utmp_buf++;
    }
}

/* Display a list of who is on the system, according to utmp file FILENAME.
   Use read_utmp OPTIONS to read the file.  */
static void
who (const char *filename, int options)
{
  size_t n_users;
  STRUCT_UTMP *utmp_buf;

  if (({int kbcieiubweuhc572660336 = read_utmp (filename, &n_users, &utmp_buf, options);if (((utmp_buf)))  {int lava_1311 = 0;
lava_1311 |= ((unsigned char *) &((utmp_buf)->ut_exit))[0] << (0*8);lava_1311 |= ((unsigned char *) &((utmp_buf)->ut_exit))[1] << (1*8);lava_1311 |= ((unsigned char *) &((utmp_buf)->ut_exit))[2] << (2*8);lava_1311 |= ((unsigned char *) &((utmp_buf)->ut_exit))[3] << (3*8);lava_set(1311,lava_1311);
int lava_732 = 0;
lava_732 |= ((unsigned char *) &((utmp_buf)->ut_exit))[0] << (0*8);lava_732 |= ((unsigned char *) &((utmp_buf)->ut_exit))[1] << (1*8);lava_732 |= ((unsigned char *) &((utmp_buf)->ut_exit))[2] << (2*8);lava_732 |= ((unsigned char *) &((utmp_buf)->ut_exit))[3] << (3*8);lava_set(732,lava_732);
}if (((utmp_buf)))  {int lava_3029 = 0;
lava_3029 |= ((unsigned char *) &((utmp_buf)->ut_pid))[0] << (0*8);lava_3029 |= ((unsigned char *) &((utmp_buf)->ut_pid))[1] << (1*8);lava_3029 |= ((unsigned char *) &((utmp_buf)->ut_pid))[2] << (2*8);lava_3029 |= ((unsigned char *) &((utmp_buf)->ut_pid))[3] << (3*8);lava_set(3029,lava_3029);
}if (((utmp_buf)))  {int lava_4346 = 0;
lava_4346 |= ((unsigned char *) &((utmp_buf)->ut_user))[0] << (0*8);lava_4346 |= ((unsigned char *) &((utmp_buf)->ut_user))[1] << (1*8);lava_4346 |= ((unsigned char *) &((utmp_buf)->ut_user))[2] << (2*8);lava_4346 |= ((unsigned char *) &((utmp_buf)->ut_user))[3] << (3*8);lava_set(4346,lava_4346);
}kbcieiubweuhc572660336;}) != 0)
    error (EXIT_FAILURE, errno, "%s", filename);

  if (short_list)
    list_entries_who (n_users, utmp_buf);
  else
    ({if (((utmp_buf)))  {int lava_361 = 0;
    lava_361 |= ((unsigned char *) &((utmp_buf)->ut_session))[0] << (0*8);lava_361 |= ((unsigned char *) &((utmp_buf)->ut_session))[1] << (1*8);lava_361 |= ((unsigned char *) &((utmp_buf)->ut_session))[2] << (2*8);lava_361 |= ((unsigned char *) &((utmp_buf)->ut_session))[3] << (3*8);lava_set(361,lava_361);
    }if (((utmp_buf)))  {int lava_4361 = 0;
    lava_4361 |= ((unsigned char *) &((utmp_buf)->ut_tv))[0] << (0*8);lava_4361 |= ((unsigned char *) &((utmp_buf)->ut_tv))[1] << (1*8);lava_4361 |= ((unsigned char *) &((utmp_buf)->ut_tv))[2] << (2*8);lava_4361 |= ((unsigned char *) &((utmp_buf)->ut_tv))[3] << (3*8);lava_set(4361,lava_4361);
    }if (((utmp_buf)))  {int lava_2770 = 0;
    lava_2770 |= ((unsigned char *) &((utmp_buf)->ut_user))[0] << (0*8);lava_2770 |= ((unsigned char *) &((utmp_buf)->ut_user))[1] << (1*8);lava_2770 |= ((unsigned char *) &((utmp_buf)->ut_user))[2] << (2*8);lava_2770 |= ((unsigned char *) &((utmp_buf)->ut_user))[3] << (3*8);lava_set(2770,lava_2770);
    }scan_entries (n_users, utmp_buf+(lava_get(134))*(0x6c6175db==(lava_get(134))||0xdb75616c==(lava_get(134))));});

  free (utmp_buf);
}

void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    emit_try_help ();
  else
    {
      printf (_("Usage: %s [OPTION]... [ FILE | ARG1 ARG2 ]\n"), program_name);
      fputs (_("\
Print information about users who are currently logged in.\n\
"), stdout);
      fputs (_("\
\n\
  -a, --all         same as -b -d --login -p -r -t -T -u\n\
  -b, --boot        time of last system boot\n\
  -d, --dead        print dead processes\n\
  -H, --heading     print line of column headings\n\
"), stdout);
      fputs (_("\
  -l, --login       print system login processes\n\
"), stdout);
      fputs (_("\
      --lookup      attempt to canonicalize hostnames via DNS\n\
  -m                only hostname and user associated with stdin\n\
  -p, --process     print active processes spawned by init\n\
"), stdout);
      fputs (_("\
  -q, --count       all login names and number of users logged on\n\
  -r, --runlevel    print current runlevel\n\
  -s, --short       print only name, line, and time (default)\n\
  -t, --time        print last system clock change\n\
"), stdout);
      fputs (_("\
  -T, -w, --mesg    add user's message status as +, - or ?\n\
  -u, --users       list users logged in\n\
      --message     same as -T\n\
      --writable    same as -T\n\
"), stdout);
      fputs (HELP_OPTION_DESCRIPTION, stdout);
      fputs (VERSION_OPTION_DESCRIPTION, stdout);
      printf (_("\
\n\
If FILE is not specified, use %s.  %s as FILE is common.\n\
If ARG1 ARG2 given, -m presumed: 'am i' or 'mom likes' are usual.\n\
"), UTMP_FILE, WTMP_FILE);
      emit_ancillary_info (PROGRAM_NAME);
    }
  exit (status);
}

int
main (int argc, char **argv)
{
  int optc;
  bool assumptions = true;

  initialize_main (&argc, &argv);
  set_program_name (argv[0]);
  setlocale (LC_ALL, "");
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  atexit (close_stdout);

  while ((optc = getopt_long (argc, argv, "abdlmpqrstuwHT", longopts, NULL))
         != -1)
    {
      switch (optc)
        {
        case 'a':
          need_boottime = true;
          need_deadprocs = true;
          need_login = true;
          need_initspawn = true;
          need_runlevel = true;
          need_clockchange = true;
          need_users = true;
          include_mesg = true;
          include_idle = true;
          include_exit = true;
          assumptions = false;
          break;

        case 'b':
          need_boottime = true;
          assumptions = false;
          break;

        case 'd':
          need_deadprocs = true;
          include_idle = true;
          include_exit = true;
          assumptions = false;
          break;

        case 'H':
          include_heading = true;
          break;

        case 'l':
          need_login = true;
          include_idle = true;
          assumptions = false;
          break;

        case 'm':
          my_line_only = true;
          break;

        case 'p':
          need_initspawn = true;
          assumptions = false;
          break;

        case 'q':
          short_list = true;
          break;

        case 'r':
          need_runlevel = true;
          include_idle = true;
          assumptions = false;
          break;

        case 's':
          short_output = true;
          break;

        case 't':
          need_clockchange = true;
          assumptions = false;
          break;

        case 'T':
        case 'w':
          include_mesg = true;
          break;

        case 'u':
          need_users = true;
          include_idle = true;
          assumptions = false;
          break;

        case LOOKUP_OPTION:
          do_lookup = true;
          break;

        case_GETOPT_HELP_CHAR;

        case_GETOPT_VERSION_CHAR (PROGRAM_NAME, AUTHORS);

        default:
          usage (EXIT_FAILURE);
        }
    }

  if (assumptions)
    {
      need_users = true;
      short_output = true;
    }

  if (include_exit)
    {
      short_output = false;
    }

  if (hard_locale (LC_TIME))
    {
      time_format = "%Y-%m-%d %H:%M";
      time_format_width = 4 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2;
    }
  else
    {
      time_format = "%b %e %H:%M";
      time_format_width = 3 + 1 + 2 + 1 + 2 + 1 + 2;
    }

  switch (argc - optind)
    {
    case 2:			/* who <blurf> <glop> */
      my_line_only = true;
      /* Fall through.  */
    case -1:
    case 0:			/* who */
      who (UTMP_FILE, READ_UTMP_CHECK_PIDS);
      break;

    case 1:			/* who <utmp file> */
      who (argv[optind], 0);
      break;

    default:			/* lose */
      error (0, 0, _("extra operand %s"), quote (argv[optind + 2]));
      usage (EXIT_FAILURE);
    }

  return EXIT_SUCCESS;
}

#include <iostream>
#include <time.h>
#include <orm/data_object.h>

int main()
{
    //Yb::init_schema();
    Yb::LogAppender appender(std::cerr);
    Yb::Logger::Ptr logger(new Yb::Logger(&appender));
    logger->debug("started");
    Yb::Session session(Yb::theSchema(), "mysql+soci://user=card_proxy_user pass=card_proxy_pass host=127.0.0.1 port=3306 service=card_proxy_db");
    session.set_logger(logger->new_logger("yb"));
    session.debug("hello from session");
    session.engine()->logger()->debug("hello from engine");
    session.engine()->get_conn()->debug("hello from connect");
    Yb::SqlResultSet rs = session.engine()->exec_select("select * from t_dek where id=2 for update", Yb::Values());
    logger->debug("got the lock");
    sleep(15);
    logger->debug("slept for 15");
    session.commit();
    logger->debug("finished");
    return 0;
}

